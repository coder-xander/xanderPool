#pragma once
#include <any>
#include <chrono>
#include <future>
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
        XQueue<TaskBasePtr> allTasks_;
        //线程
        std::thread thread_;
        std::atomic_bool exitflag_;
        //任务计数信号量
        SemaphoreGuard taskSemaphoreGuard_;


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
        bool  isBusy()
        {
            return allTasks_.empty() == false;
        }
        static std::shared_ptr<Worker> makeShared()
        {
            return std::make_shared<Worker>();
        }
        Worker() : taskSemaphoreGuard_(0)
        {
            exitflag_.store(false);
            thread_ = std::thread([this]()
                {
                    while (!exitflag_)
                    {
                        taskSemaphoreGuard_.consume();
                        executeFirst();
                    }
                    // state_.store(Shutdown);
                });
        }
        ~Worker()
        {
            std::cout << "~Worker" << std::endl;
        }
        size_t generateTaskId()
        {
            auto duration = std::chrono::system_clock::now().time_since_epoch();
            auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
            auto taskId = std::hash<decltype(now)>()(now);
            while (findTask(taskId) != nullptr)
            {
                taskId = generateTaskId();
            }
            return taskId;
        }

        template <typename F, typename... Args, typename  R = typename  std::invoke_result_t<F, Args...>>
        TaskResultPtr<R> submit(F&& function, Args &&...args)
        {
            size_t taskId = generateTaskId();
            auto task = std::make_shared<Task<F, R, Args...>>(taskId, std::forward<F>(function), std::forward<Args>(args)...);
            allTasks_.enqueue(task);
            TaskResultPtr taskResultPtr = TaskResult<R>::makeShared(taskId, std::move(task->getTaskPackaged().get_future()));
            task->setTaskResult(taskResultPtr);
            taskSemaphoreGuard_.release();
            return taskResultPtr;
        }

        TaskBasePtr  executeFirst()
        {
            auto taskOpt = allTasks_.tryPop();
            if (taskOpt.has_value())
            {
                auto taskPtr = taskOpt.value()->run();
                return   taskPtr;
            }
            return nullptr;
        }
        TaskBasePtr findTask(size_t taskId)
        {
            return allTasks_.find([taskId](auto task)
                { return task->getId() == taskId; });
        }
        bool removeTask(size_t taskId);
        size_t getTaskCount() { return allTasks_.size(); }
        void clear();
    };
    using WorkerPtr = std::shared_ptr<Worker>;

}
