#pragma once
#include <any>
#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <memory>
#include <iostream>

/// @brief 执行结果的返回值
/// @author xander
class TaskResult
{
public:
    static std::shared_ptr<TaskResult> makeShard(size_t id, std::shared_ptr<std::promise<std::any>> resultFture)
    {
        return std::make_shared<TaskResult>(id, resultFture);
    }
    TaskResult(size_t id, std::shared_ptr<std::promise<std::any>> resultFuture) : id_(id), resultFuture_(resultFuture) {}
    auto getId() const { return id_; }
    ~TaskResult() { std::cout << " ~ExecuteResult" << std::endl; }
    const std::any& toAny() const { return resultFuture_->get_future().get(); }
    void setId(size_t id) { id_ = id; };
    template <typename T>
    T to() const
    {
      
        if (resultFuture_->get_future().get().type() != typeid(T))
        {
            throw std::bad_cast();
        }
        return std::any_cast<T>(resultFuture_->get_future().get());
    }
    std::string toString() const
    {
        return to<std::string>();
    }
    int toInt() const
    {
        return to<int>();
    }
    float toFloat() const
    {
        return to<float>();
    }
    double toDouble() const
    {
        return to<double>();
    }
    bool toBool() const
    {
        return to<bool>();
    }
    std::shared_ptr<std::promise<std::any>>getResultFuture()
    {
        std::lock_guard lock(resultFutureMutex_);
        return resultFuture_;
    }
private:
    std::optional<size_t> id_;
    std::mutex resultFutureMutex_;
    std::shared_ptr<std::promise<std::any>> resultFuture_;
};
using TaskResultPtr = std::shared_ptr<TaskResult>;
