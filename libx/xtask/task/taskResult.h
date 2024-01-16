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
    static std::shared_ptr<TaskResult> makeShard(size_t id, std::future<std::any>&& future)
    {
        return std::make_shared<TaskResult>(id, std::move(future));
    }
    TaskResult(size_t id, std::future<std::any>&& future) : id_(id), future_(std::move(future))
    {

    }
    auto getId() const { return id_; }
    ~TaskResult() { std::cout << " ~ExecuteResult" << std::endl; }
    void setId(size_t id) { id_ = id; };
    template <typename T>
    T to()
    {
        auto r = future_.get();
        if (r.type() != typeid(T))
        {
            throw std::bad_cast();
        }
        return std::any_cast<T>(r);
    }
    std::string toString()
    {
        return to<std::string>();
    }
    int toInt()
    {
        return to<int>();
    }
    float toFloat()
    {
        return to<float>();
    }
    double toDouble()
    {
        return to<double>();
    }
    bool toBool()
    {
        return to<bool>();
    }

private:
    size_t id_;
    std::future<std::any> future_;
};
using TaskResultPtr = std::shared_ptr<TaskResult>;
// using TaskResultWeakPtr = std::weak_ptr<TaskResult>;
