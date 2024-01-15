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
    static std::shared_ptr<TaskResult> makeShard(size_t id, std::shared_ptr<std::promise<std::any>> resultPromise)
    {
        return std::make_shared<TaskResult>(id, resultPromise);
    }
    TaskResult(size_t id, std::shared_ptr<std::promise<std::any>> resultPromise) : id_(id), resultPromise(resultPromise)
    {
        future_ = resultPromise->get_future();
    }
    auto getId() const { return id_; }
    ~TaskResult() { std::cout << " ~ExecuteResult" << std::endl; }
    void setId(size_t id) { id_ = id; };
    template <typename T>
    T to() 
    {

        if (future_.get().type() != typeid(T))
        {
            throw std::bad_cast();
        }
        return std::any_cast<T>(future_.get());
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
    std::shared_ptr<std::promise<std::any>>getResultPromise()
    {
        std::lock_guard lock(resultPromiseMutex_);
        return resultPromise;
    }
private:
    size_t id_;
    std::mutex resultPromiseMutex_;
    std::shared_ptr<std::promise<std::any>> resultPromise;
    std::future<std::any> future_;
};
using TaskResultPtr = std::shared_ptr<TaskResult>;
// using TaskResultWeakPtr = std::weak_ptr<TaskResult>;
