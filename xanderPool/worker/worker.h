#pragma once
#include <any>
#include <chrono>
#include <future>
#include <random>
#include <sstream>

#include "../queue/queue.h"
#include "../task/taskResult.h"
#include "../task/task.h"
#include "../semaphoreGuard/semaphoreGuard.h"
///@brief  任务worker拥有一个线程、一个任队列，这是一个线程池的基本单元，所有方法线程安全

namespace xander
{

    class Worker
    {
    public:
        // enum States
        // {
        //     Idle = 0, Busy, Shutdown
        // };
    private:
        //名字
        // std::shared_mutex nameMutex_;
        // std::string name_;
        //状态
        // std::atomic<States> state_;
        //三个优先级任务队列
        XDeque<TaskBasePtr> tasks_;
        XDeque<TaskBasePtr> HighPriorityTasks_;
        XDeque<TaskBasePtr> LowPriorityTasks_;
        //线程
        std::thread thread_;
        std::mutex threadMutex_;
        std::atomic_bool exitflag_;
        //任务计数信号量
        std::condition_variable taskCv_;
        std::mutex tasksMutex_;
        // SemaphoreGuard taskSemaphoreGuard_;
        //shutDown
        std::condition_variable shutdownCv_;
        std::mutex shutdownMutex_;
    private:
        bool allTaskDequeEmpty()
        {
            return tasks_.empty()&&HighPriorityTasks_.empty()&&LowPriorityTasks_.empty();
        }
    public:
        // void setName(std::string name)
        // {
        //     std::lock_guard lock(nameMutex_);
        //     name_ = name;
        // }
        // std::string getName()
        // {
        //     std::lock_guard lock(nameMutex_);
        //     return name_;
        // }
        std::string idString()
        {
            std::ostringstream os;
            os << thread_.get_id();
            return os.str();
        }
        bool  isBusy() const
        {
            return !tasks_.empty()||!HighPriorityTasks_.empty()||!LowPriorityTasks_.empty() ;
        }
        static std::shared_ptr<Worker> makeShared()
        {
            return std::make_shared<Worker>();
        }
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
                            taskCv_.wait(lock);
                        }
                        if (exitflag_)
                        {
                            shutdownCv_.notify_one();
                            break;
                        }
                        executePop();
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
        ~Worker()
        {
            // shutdown();
            std::cout << "~Worker" << std::endl;
        }
        /// @brief获得一个优先级最高的任务
        std::optional<TaskBasePtr> decideHighestPriorityTask()
        {
            
            if (HighPriorityTasks_.empty() == false)
            {
               return  HighPriorityTasks_.tryPop();
            }
            else if (tasks_.empty() == false)
            {
                return tasks_.tryPop();
            }
            else if (LowPriorityTasks_.empty() == false)
            {
                return LowPriorityTasks_.tryPop();
            }
            return std::nullopt;
        }
        //耗时操作
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
        template <typename F, typename... Args, typename  R = typename  std::invoke_result_t<F, Args...>>
        TaskResultPtr<R> submit(F&& function, Args &&...args,const TaskBase::Priority & priority)
        {
            // auto taskId = generateUUID();
            std::string taskId = "";
            auto task = std::make_shared<Task<F, R, Args...>>(taskId, std::forward<F>(function), std::forward<Args>(args)...);
            TaskResultPtr taskResultPtr = TaskResult<R>::makeShared(taskId, std::move(task->getTaskPackaged().get_future()));
            task->setPriority(priority);
            task->setTaskResult(taskResultPtr);
            taskResultPtr->setTask(task);
            enQueueTaskByPriority(task);//入队
            taskCv_.notify_one();
            return taskResultPtr;
        }
        /// @brief 按照优先级入队
        void enQueueTaskByPriority(TaskBasePtr task)
        {
            if (task->priority() == TaskBase::Normal)
            {
                               tasks_.enqueue(task);

            }
            else if (task->priority() == TaskBase::High)
            {
                               HighPriorityTasks_.enqueue(task);
            }
            else if (task->priority() == TaskBase::low)
            {
                               LowPriorityTasks_.enqueue(task);
            }
            taskCv_.notify_one();
        }
        TaskBasePtr  executePop()
        {
            auto taskOpt = decideHighestPriorityTask();
            if (taskOpt.has_value())
            {
                auto taskPtr = taskOpt.value()->run();
                return   taskPtr;
            }
            return nullptr;
        }
        
        [[maybe_unused]]  TaskBasePtr findTask(const std::string& taskId)
        {
            auto op = tasks_.find([taskId](auto task)
                { return task->getId() == taskId; });
            if (op.has_value())
            {
                return op.value();
            }
            else
            {
                return nullptr;
            }
        }
        [[maybe_unused]] bool removeTask(size_t taskId);
        size_t getTaskCount() { return tasks_.size(); }
        [[maybe_unused]] void clear();
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

    };
    using WorkerPtr = std::shared_ptr<Worker>;

}
