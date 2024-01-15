#pragma once
#include "taskResult.h"
#include <memory>
#include <functional>
#include <any>
#include <future>
/// @brief 任务的基类
class TaskBase
{
public:
    virtual ~TaskBase() = default;
    virtual std::any run() = 0;
    virtual size_t getId() = 0;
    virtual void  setTaskResult(TaskResultPtr taskResultPtr) = 0;
    virtual TaskResultPtr getTaskResult() = 0;
};

/// @brief 任务实现类
/// @tparam F 函数
/// @tparam ...Args 参数
/// @tparam R 返回值
template <typename F, typename R, typename... Args>
class Task final : public TaskBase
{
public:
    explicit Task(size_t id, F&& function, Args &&...args)
        : id_(id), func_(std::bind(std::forward<F>(function), std::forward<Args>(args)...)) {}
    ~Task() override
    {
        std::cout << " ~Task" << std::endl;
    }
    std::any run() override
    {
        return func_();
    }
    size_t getId() override
    {
        return id_;
    }
    void setTaskResult(TaskResultPtr taskResultPtr) override
    {
       taskResultPtr_ = taskResultPtr;
    }
    TaskResultPtr getTaskResult() override
    {

        return taskResultPtr_;
    }
private:
    size_t id_;
    std::function<R()> func_;
    TaskResultPtr taskResultPtr_;
};
using TaskBasePtr = std::shared_ptr<TaskBase>;
