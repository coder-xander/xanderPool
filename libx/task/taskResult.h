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

    class TaskResult
    {
    public:
        static std::shared_ptr<TaskResult> makeShared(size_t id, std::future<std::any>&& future)
        {
            return std::make_shared<TaskResult>(id, std::move(future));
        }
        static std::shared_ptr<TaskResult> makeShared(size_t id, std::future<void>&& future)
        {
            return std::make_shared<TaskResult>(id, std::move(future));
        }

        TaskResult(size_t id, std::future<void>&& future) : id_(id), voidFuture_(std::move(future))
        {

        }
        TaskResult(size_t id, std::future<std::any>&& future) : id_(id), anyFuture_(std::move(future))
        {

        }
        auto getId() const { return id_; }
        ~TaskResult()
        {
            // std::cout << " ~ExecuteResult" << std::endl;
        }
        void setId(size_t id) { id_ = id; };


        template <typename T>
        std::optional<T> to()
        {
            if (anyFuture_.valid())
            {
                auto r = anyFuture_.get();
                if (r.type() != typeid(T))
                {
                    throw std::bad_cast();
                }
                return std::any_cast<T>(r);
            }
            else
            {
                return std::nullopt;
            }
        }

        std::optional<std::monostate> toVoid() {
            if (voidFuture_.valid()) {
                voidFuture_.get(); // Wait for the future to be ready
                return std::monostate();
            }
            else {
                return std::nullopt;
            }
        }
        std::optional<std::string> toString()
        {
            return to<std::string>();
        }

        std::optional<int> toInt()
        {
            return to<int>();
        }

        std::optional<float> toFloat()
        {
            return to<float>();
        }

        std::optional<double> toDouble()
        {
            return to<double>();
        }

        std::optional<bool> toBool()
        {
            return to<bool>();
        }

    private:
        size_t id_;
        std::future<std::any> anyFuture_;
        std::future<void> voidFuture_;
    };
    using TaskResultPtr = std::shared_ptr<TaskResult>;
    // using TaskResultWeakPtr = std::weak_ptr<TaskResult>;

}