#pragma once
#include "taskResult.h"
#include "taskId.h"
#include <memory>
#include <functional>
#include <any>
/// @brief 任务的基类

class TaskBase
{
public:
    virtual ~TaskBase() = default;
    virtual ExcuteResultPtr run() = 0;
    virtual TaskIdPtr getId() = 0;
};

/// @brief 任务实现类
/// @tparam F 函数
/// @tparam ...Args 参数
/// @tparam R 返回值
template <typename F, typename R, typename... Args>
class Task : public TaskBase
{
public:
    explicit Task(TaskIdPtr id, F&& function, Args &&...args)
        : id_(id), func_(std::bind(std::forward<F>(function), std::forward<Args>(args)...)) {}
    ~Task() override
    {
        std::cout << " ~Task" << std::endl;
    }
    ExcuteResultPtr run() override
    {
        return std::make_shared<ExecuteResult>(id_, func_());
    }
    TaskIdPtr getId() override
    {
        return id_;
    }

private:
    TaskIdPtr id_;
    std::function<R()> func_;
    std::any result_;
};
using ITaskPtr = std::shared_ptr<TaskBase>;
