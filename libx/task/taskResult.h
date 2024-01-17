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


        R value()
        {
            return future_.get();
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