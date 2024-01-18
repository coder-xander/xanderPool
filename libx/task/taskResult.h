#pragma once
#include <any>
#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <memory>
/// @brief 执行结果的返回值
/// @author xander
namespace xander
{
    template<typename  R>
    class TaskResult
    {
    public:
        static std::shared_ptr<TaskResult> makeShared(size_t id, std::future<R>&& future)
        {
            return std::make_shared<TaskResult>(id, std::move(future));
        }


        TaskResult(size_t id, std::future<R>&& future) : id_(id), future_(std::move(future))
        {

        }
        auto getId() const { return id_; }
        ~TaskResult()
        {
            // std::cout << " ~ExecuteResult" << std::endl;
        }
        void setId(size_t id) { id_ = id; };
        //同步获取结果,会一直等待，直到结果准备好
        R syncGetValue()
        {
            future_.wait();
            return future_.get();
        }
        //同步获取结果，如果结果准备好，会立即返回，否则会等待，超时会返回
        std::conditional_t<std::is_same_v<void, R>, void, std::optional<R>> syncGetValue(int ms)
        {
            auto time = std::chrono::milliseconds(ms);
            auto waitR = future_.wait_for(time);
            if (waitR == std::future_status::ready)
            {
                if constexpr (std::is_same_v<void, R>)
                {
                    future_.get(); //Acknowledge the ready status.
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
            }
        }

        // std::optional<std::monostate> toVoid() {
        //     if (voidFuture_.valid()) {
        //         voidFuture_.get(); // Wait for the future to be ready
        //         return std::monostate();
        //     }
        //     else {
        //         return std::nullopt;
        //     }
        // }

    private:
        size_t id_;
        std::future<R> future_;
    };
    template<typename R>
    using TaskResultPtr = std::shared_ptr<TaskResult<R>>;
    // using TaskResultWeakPtr = std::weak_ptr<TaskResult>;

}