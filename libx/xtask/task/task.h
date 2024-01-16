﻿#pragma once
#include <memory>
#include <functional>
#include <any>
#include <future>
#include "taskResult.h"

/// @brief 任务的基类
class TaskBase :public  std::enable_shared_from_this<TaskBase>
{
public:
    virtual ~TaskBase() = default;
    virtual std::shared_ptr<TaskBase> run() = 0;
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
        : id_(id), packagedFunc_(std::bind(std::forward<F>(function), std::forward<Args>(args)...))
    {
    }
    ~Task() override
    {
        // std::cout << " ~Task" << std::endl;
    }
    std::shared_ptr<TaskBase>  run() override
    {
        std::lock_guard lock(taskResultMutex_);
        packagedFunc_();
        return shared_from_this();
    }
    size_t getId() override
    {
        return id_;
    }
    void setTaskResult(TaskResultPtr taskResultPtr) override
    {
        std::lock_guard lock(taskResultMutex_);
        taskResultPtr_ = taskResultPtr;
    }
    TaskResultPtr getTaskResult() override
    {
        std::lock_guard lock(taskResultMutex_);
        return taskResultPtr_;
    }
    std::packaged_task<std::any()>& getTaskPackaged()
    {
        return packagedFunc_;
    }
private:
    size_t id_;
    std::packaged_task<std::any()> packagedFunc_;
    TaskResultPtr taskResultPtr_;
    std::mutex taskResultMutex_;
};
using TaskBasePtr = std::shared_ptr<TaskBase>;