#pragma once
#include <any>
#include <functional>
#include <future>

#include "taskId.h"
#include <iostream>
/// @brief 执行结果的返回值
/// @author xander
class TaskResult
{
public:
    static std::shared_ptr<TaskResult> makeShard(TaskIdPtr id, std::shared_ptr<std::promise<std::any>> resultFture)
    {
        return std::make_shared<TaskResult>(id, resultFture);
    }
    TaskResult(TaskIdPtr id, std::shared_ptr<std::promise<std::any>> resultFuture) : id_(id), resultFuture_(resultFuture) {}
    TaskResult(TaskId id, std::shared_ptr<std::promise<std::any>> resultFuture) : id_(std::make_shared<TaskId>(id)), resultFuture_(resultFuture) {}
    auto getId() const { return id_; }
    ~TaskResult() { std::cout << " ~ExecuteResult" << std::endl; }
    const std::any& toAny() const { return resultFuture_->get_future().get(); }
    void setId(TaskIdPtr id) { id_ = id; };
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
        return resultFuture_;
    }
private:
    TaskIdPtr id_;
    std::shared_ptr<std::promise<std::any>> resultFuture_;
};
using TaskResultPtr = std::shared_ptr<TaskResult>;
