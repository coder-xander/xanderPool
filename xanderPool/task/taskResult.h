#pragma once
#include <future>
#include <iostream>
#include <optional>
#include <memory>
/// @brief 执行结果的返回值
/// @author xander
namespace xander
{
    class TaskBase;
    template<typename  R>
    class TaskResult
    {
    public:
        static std::shared_ptr<TaskResult> makeShared(std::string& id, std::future<R>&& future)
        {
            return std::make_shared<TaskResult>(id, std::move(future));
        }

        
        TaskResult(std::string& id, std::future<R>&& future) : id_(id), future_(std::move(future))
        {

        }
        auto getId() const { return id_; }
        ~TaskResult()
        {
            // std::cout << " ~ExecuteResult" << std::endl;
        }
        void setId(std::string& id) { id_ = id; };
        ///@brief 同步获取结果,会一直等待，直到结果准备好
        R syncGetValue()
        {
            future_.wait();
            return future_.get();
        }
        /// @brief 在预期时间内同步获取结果
        /// @param timeout 超时时间，单位毫秒
        /// @return 如果结果准备好，会立即返回optional，如果超时，返回std::nullopt,注意:void结果返回nullopt
        std::conditional_t<std::is_same_v<void, R>, void, std::optional<R>> syncGetValue(int timeout)
        {
            auto time = std::chrono::milliseconds(timeout);
            auto waitR = future_.wait_for(time);
            if (waitR == std::future_status::ready)
            {
                if constexpr (std::is_same_v<void, R>)
                {
                    future_.get(); //return void
                    return;
                }
                else
                {
                    return future_.get();
                }
            }
            else
            {
                std::cout << "wait for result timeout" << std::endl;
                if constexpr (!std::is_same_v<void, R>)
                {
                    return  std::nullopt;
                }
                //else return void 
            }
        }

        void setTask(std::weak_ptr<TaskBase> task)
        {
            task_ = task;
        }
        auto  task()
        {
            return task_;
        }
        // template <typename F, typename... Args, typename  R_ = typename  std::invoke_result_t<F, Args...>>
        // std::shared_ptr<R_> then(F&& function, Args &&...args, const TaskBase::Priority& priority)
        // {
        //     // auto taskId = generateUUID();
        //     std::string taskId = "";
        //     auto task = std::make_shared<Task<F, R_, Args...>>(taskId, std::forward<F>(function), std::forward<Args>(args)...);
        //     auto taskResultPtr = TaskResult<R>::makeShared(taskId, std::move(task->getTaskPackaged().get_future()));
        //     task->setPriority(priority);
        //     task->setTaskResult(taskResultPtr);
        //     taskResultPtr->setTask(task);
        //     enQueueTaskByPriority(task);//入队
        //     taskCv_.notify_one();
        //     return taskResultPtr;
        // }

    private:
        std::string  id_;
        std::future<R> future_;
        std::weak_ptr<TaskBase> task_;
    };
    template<typename R>
    using TaskResultPtr = std::shared_ptr<TaskResult<R>>;
    // using TaskResultWeakPtr = std::weak_ptr<TaskResult>;

}
//预声明。
#include "task.h"