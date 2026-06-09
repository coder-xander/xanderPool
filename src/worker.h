#pragma once
#include <chrono>
#include <random>
#include <sstream>
#include "queue.h"
#include "taskResult.h"
#include "task.h"
#include <thread>

namespace xander
{

    class Worker
    {

    private:
        //task queue
        XDeque<TaskBasePtr> normalTasks_;
        XDeque<TaskBasePtr> highPriorityTasks_;
        XDeque<TaskBasePtr> lowPriorityTasks_;
        //thread
        std::thread thread_;
        std::mutex threadMutex_;
        std::atomic_bool exitflag_;
        std::atomic_bool isBusy_;

    private:
        /// @brief  if all task deque is empty
        bool allTaskDequeEmpty()
        {
            return normalTasks_.empty() && highPriorityTasks_.empty() && lowPriorityTasks_.empty();
        }
    public:
        ///@brief create a new worker decorated by shared_ptr
        static std::shared_ptr<Worker> makeShared()
        {
            return std::make_shared<Worker>();
        }
        ///@brief destructor,ps:all workers will destroyed when thread pool destruct or destroy by automatic garbage collector.
        ~Worker()
        {
        }
        ///@brief constructor，create a thread to run task,isBusy flag will be dynamic set,so we know worker`s state
        Worker()
        {
            exitflag_.store(false);
            isBusy_.store(false);
            std::lock_guard threadLock(threadMutex_);
            thread_ = std::thread([this]()
                {
                    while (true)
                    {
                        // 自旋等待：队列空且未退出时 yield，避免 condition_variable 竞态
                        while (allTaskDequeEmpty() && !exitflag_.load())
                        {
                            isBusy_.store(false);
                            std::this_thread::yield();
                        }

                        if (exitflag_)
                        {
                            break;
                        }

                        isBusy_.store(true);
                        //run task
                        execute();

                        if (exitflag_)
                        {
                            break;
                        }

                    }
                });
        }

        ///@brief find tasks by name
        std::vector<TaskBasePtr> findTasks(const std::string& name)
        {
            std::vector<TaskBasePtr> tasks;
            std::unique_lock<std::mutex> lock(normalTasks_.mutex());
            for (auto& task : normalTasks_.deque())
            {
                if (task->name() == name)
                {
                    tasks.push_back(task);
                }
            }
            lock.unlock();
            std::unique_lock<std::mutex> lock1(highPriorityTasks_.mutex());
            for (auto& task : highPriorityTasks_.deque())
            {
                if (task->name() == name)
                {
                    tasks.push_back(task);
                }
            }
            lock1.unlock();
            std::unique_lock<std::mutex> lock2(lowPriorityTasks_.mutex());
            for (auto& task : lowPriorityTasks_.deque())
            {
                if (task->name() == name)
                {
                    tasks.push_back(task);
                }
            }
            lock2.unlock();
            return tasks;
        }
        ///@brief force shutdown worker and forgive all task in queue whether it is finished or not
        [[maybe_unused]]
        bool shutdown()
        {
            exitflag_.store(true);
            std::lock_guard threadLock(threadMutex_);
            if (thread_.joinable())
            {
                thread_.join();
                return true;
            }
            return true;
        }
        ///@brief get string id
        std::string idString() const {
            std::ostringstream os;
            os << thread_.get_id();
            return os.str();
        }
        ///@brief get result of if worker is on work
        bool isBusy() const
        {
            return isBusy_.load();
        }
        /// @brief decide a highest priority task
        std::optional<TaskBasePtr> decideHighestPriorityTask()
        {
            if (!highPriorityTasks_.empty())
            {
                return highPriorityTasks_.tryPop();
            }
            if (!normalTasks_.empty())
            {
                return normalTasks_.tryPop();
            }
            if (!lowPriorityTasks_.empty())
            {
                return lowPriorityTasks_.tryPop();
            }
            return std::nullopt;
        }
        ///@brief generate a uuid
        static std::string generateUUID() {
            std::random_device rd;
            std::mt19937 rng(rd());
            std::uniform_int_distribution<> uni(0, 15);

            const char* chars = "0123456789ABCDEF";
            const char* uuidTemplate = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";

            std::string uuid(uuidTemplate);

            for (auto& c : uuid) {
                if (c != 'x' && c != 'y') {
                    continue;
                }
                const int r = uni(rng);
                c = chars[(c == 'y' ? (r & 0x3) | 0x8 : r)];
            }

            return uuid;
        }
        ///@brief submit a task with priority
        template <typename F, typename... Args, typename R = typename std::invoke_result_t<F, Args...>>
        TaskResultPtr<R> submit(const TaskBase::Priority& priority, F&& function, Args&&...args)
        {
            auto task = makeTask(priority, std::forward<F>(function), std::forward<Args>(args)...);
            enQueueTaskByPriority(task);
            return task->getTaskResult();
        }
        ///@brief submit a task with default priority
        template <typename F, typename... Args, typename R = typename std::invoke_result_t<F, Args...>>
        TaskResultPtr<R> submit(F&& function, Args&&...args)
        {
            auto task = makeTask(TaskBase::Normal, std::forward<F>(function), std::forward<Args>(args)...);
            enQueueTaskByPriority(task);
            return task->getTaskResult();
        }
        ///@brief submit a pre-made task with priority
        template <typename F, typename... Args, typename R = typename std::invoke_result_t<F, Args...>>
        TaskResultPtr<R> submit(TaskPtr<F, R, Args...> task)
        {
            enQueueTaskByPriority(task);
            return task->getTaskResult();
        }

        ///@brief submit a TaskBasePtr (used by submitSome)
        void submit(TaskBasePtr task)
        {
            enQueueTaskByPriority(task);
        }

        /// @brief enqueue by priority.
        void enQueueTaskByPriority(TaskBasePtr task)
        {
            if (task->priority() == TaskBase::Normal)
            {
                normalTasks_.enqueue(task);
                isBusy_.store(true);
                return;
            }
            if (task->priority() == TaskBase::High)
            {
                highPriorityTasks_.enqueue(task);
                isBusy_.store(true);
                return;
            }
            if (task->priority() == TaskBase::low)
            {
                lowPriorityTasks_.enqueue(task);
                isBusy_.store(true);
                return;
            }
        }
        ///@brief execute a task
        void execute()
        {
            auto taskOpt = decideHighestPriorityTask();
            if (taskOpt.has_value())
            {
                auto task = taskOpt.value();
                task->run();
            }
        }

        size_t taskCount() { return normalTasks_.size() + highPriorityTasks_.size() + lowPriorityTasks_.size(); }
        size_t normalPriorityTaskCount() { return normalTasks_.size(); }
        size_t highPriorityTaskCount() { return highPriorityTasks_.size(); }
        size_t lowPriorityTaskCount() { return lowPriorityTasks_.size(); }
        void clear() { normalTasks_.clear(); highPriorityTasks_.clear(); lowPriorityTasks_.clear(); }

    };
    using WorkerPtr = std::shared_ptr<Worker>;

}
