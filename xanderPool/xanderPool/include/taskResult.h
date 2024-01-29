#pragma once
#include <future>
#include <iostream>
#include <optional>
#include <memory>
/// @brief the result of task execute 
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
        static std::shared_ptr<TaskResult> makeShared(std::future<R>&& future)
        {
            return std::make_shared<TaskResult>(std::move(future));
        }


        TaskResult(std::future<R>&& future) : future_(std::move(future))
        {

        }

        ~TaskResult()
        {
            // std::cout << " ~ExecuteResult" << std::endl;
        }

        ///@brief sync get result of task,will wait until result is ready,if result is void,return void,can only call once.
        auto  syncGetResult()
        {
            future_.wait();
            return future_.get();
        }
        /// @brief sync get result of task in timeout,if result is void,return void,can only call once.
        /// @param timeout 
        /// @return if result is ready,return optional immediately,if timeout,return std::nullopt,notice:if result is void,return nullopt
        std::conditional_t<std::is_same_v<void, R>, void, std::optional<R>> syncGetResult(int timeout)
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
            std::cout << "wait for result timeout" << std::endl;
            if constexpr (!std::is_same_v<void, R>)
            {
                return  std::nullopt;
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


    private:

        std::future<R> future_;
        std::weak_ptr<TaskBase> task_;
    };
    template<typename R>
    using TaskResultPtr = std::shared_ptr<TaskResult<R>>;
    // using TaskResultWeakPtr = std::weak_ptr<TaskResult>;

}
#include "task.h"