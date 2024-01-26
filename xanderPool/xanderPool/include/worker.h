#pragma once
#include <any>
#include <chrono>
#include <random>
#include <sstream>
#include "queue.h"
#include "taskResult.h"
#include "task.h"
///@brief  worker is a thread to run task,it container three priority queue of different priority task.

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
        //task cv
        std::condition_variable taskCv_;
        std::mutex tasksMutex_;
        //shutDown
        std::condition_variable shutdownCv_;
        std::mutex shutdownMutex_;
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
            // shutdown();
            std::cout << "~Worker" << std::endl;
        }
        ///@brief constructor，create a thread to run task,isBusy flag will be dynamic set,so we know worker`s state
        Worker()
        {
            exitflag_.store(false);
            std::lock_guard threadLock(threadMutex_);
            thread_ = std::thread([this]()
                {
                    while (true)
                    {
                        if (allTaskDequeEmpty())
                        {

                            std::unique_lock<std::mutex>  lock(tasksMutex_);
                            isBusy_.store(false);
                            taskCv_.wait(lock);
                        }
                        if (exitflag_)
                        {
                            shutdownCv_.notify_one();
                            break;
                        }
                        isBusy_.store(true);
                        //run task
                        auto task = execute();

                        if (exitflag_)
                        {
                            shutdownCv_.notify_one();
                            break;
                        }

                    }
                    // state_.store(Shutdown);
                    std::cout << "worker thread exit" << std::endl;
                });
        }
        ///@brief get string id 
        std::string idString()
        {
            std::ostringstream os;
            os << thread_.get_id();
            return os.str();
        }
        ///@brief get result of if worker is on work 
        bool  isBusy() const
        {
            return isBusy_.load();
        }
        /// @brief decide a highest priority task
        std::optional<TaskBasePtr> decideHighestPriorityTask()
        {

            if (highPriorityTasks_.empty() == false)
            {
                return  highPriorityTasks_.tryPop();
            }
            else if (normalTasks_.empty() == false)
            {
                return normalTasks_.tryPop();
            }
            else if (lowPriorityTasks_.empty() == false)
            {
                return lowPriorityTasks_.tryPop();
            }
            return std::nullopt;
        }
        ///@brief generate a uuid,but this operation cost more time
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
        ///@brief submit a task ,it maybe global function、lambda、member function
        template <typename F, typename... Args, typename  R = typename  std::invoke_result_t<F, Args...>>
        TaskResultPtr<R> submit(F&& function, Args &&...args, const TaskBase::Priority& priority)
        {

            auto task = std::make_shared<Task<F, R, Args...>>(std::forward<F>(function), std::forward<Args>(args)...);
            TaskResultPtr<R> taskResultPtr = TaskResult<R>::makeShared(std::move(task->getTaskPackaged().get_future()));
            task->setPriority(priority);
            task->setTaskResult(taskResultPtr);
            taskResultPtr->setTask(task);
            enQueueTaskByPriority(task);//入队
            taskCv_.notify_one();
            return taskResultPtr;
        }
        /// @brief enqueue by priority.
        void enQueueTaskByPriority(TaskBasePtr task)
        {
            if (task->priority() == TaskBase::Normal)
            {
                normalTasks_.enqueue(task);
                taskCv_.notify_one();
                return;
            }
            if (task->priority() == TaskBase::High)
            {
                highPriorityTasks_.enqueue(task);
                taskCv_.notify_one();
                return;
            }
            if (task->priority() == TaskBase::low)
            {
                lowPriorityTasks_.enqueue(task);
                taskCv_.notify_one();
                return;
            }

        }
        ///@brief execute a task and it`s all linked task
        TaskBasePtr  execute()
        {
            auto taskOpt = decideHighestPriorityTask();//起始task
            if (taskOpt.has_value())
            {
                auto task = taskOpt.value();
                task->run();
            }
            else
            {
                return  nullptr;
            }

            return nullptr;
        }
        ///@brief force shutdown worker and forgive all task in queue weather it is finished or not
        [[maybe_unused]]
        bool shutdown()
        {
            exitflag_.store(true);
            std::unique_lock   lock(shutdownMutex_);
            taskCv_.notify_one();
            shutdownCv_.wait(lock);
            std::lock_guard threadLock(threadMutex_);
            if (thread_.joinable())
            {
                thread_.join();
                return true;
            }
            return true;
        }

        [[maybe_unused]] bool removeTask(size_t taskId);
        size_t taskCount() { return normalTasks_.size() + highPriorityTasks_.size() + lowPriorityTasks_.size(); }
        size_t normalPriorityTaskCount() { return normalTasks_.size(); }
        size_t highPriorityTaskCount() { return highPriorityTasks_.size(); }
        size_t lowPriorityTaskCount() { return lowPriorityTasks_.size(); }
        [[maybe_unused]] void clear()
        {

            normalTasks_.clear();
        }

    };
    using WorkerPtr = std::shared_ptr<Worker>;

}
