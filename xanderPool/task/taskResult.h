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
    template <typename F, typename R, typename... Args >
    class Task;
    template<typename  R>
    class TaskResult
    {
    public:
        static std::shared_ptr<TaskResult> makeShared( std::future<R>&& future)
        {
            return std::make_shared<TaskResult>( std::move(future));
        }

        
        TaskResult( std::future<R>&& future) :  future_(std::move(future))
        {

        }
       
        ~TaskResult()
        {
            // std::cout << " ~ExecuteResult" << std::endl;
        }
    
        ///@brief 同步获取结果,会一直等待，直到结果准备好，如果结果是void，返回void，只能调用一次。
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
        template <typename F, typename... Args, typename  R_ = typename  std::invoke_result_t<F, R, Args...>>
        R_ functionHelper(F&& function,R v ,Args &&...args)
        {

          return   function(v, std::forward<Args>(args)...);
        }
        ///@brief setting the next task when this task finished,this function will return a new result ,so you can call this function chainly.
        template <typename F, typename... Args, typename  R_ = typename  std::invoke_result_t<F, R, Args...>>
        std::shared_ptr<TaskResult<R_>> then(F&& function, Args&&... args)
        {
            R v = syncGetValue();
            std::packaged_task<R_(Args...)> packagedTask(std::bind(std::forward<F>(function), v, std::forward<Args>(args)...));
            auto task = std::make_shared<Task<F, R_, Args...>>(std::move(packagedTask));
            auto taskResultPtr = TaskResult<R_>::makeShared(std::move(task->getTaskPackaged().get_future()));
            task->setTaskResult(taskResultPtr);
            taskResultPtr->setTask(task);
            task_.lock()->setNextRelatedTask(task);
            return taskResultPtr;
        }




    private:
     
        std::future<R> future_;
        std::weak_ptr<TaskBase> task_;
    };
    template<typename R>
    using TaskResultPtr = std::shared_ptr<TaskResult<R>>;
    // using TaskResultWeakPtr = std::weak_ptr<TaskResult>;

}
//预声明。
#include "task.h"