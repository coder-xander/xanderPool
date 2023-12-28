#pragma once
/// @brief 任务管理类

#include "../task/task.h"
#include <chrono>
#include "../../xqueue/xqueue.h"
class TaskManager
{
public:
    static std::shared_ptr<TaskManager> makeShared()
    {
        return std::make_shared<TaskManager>();
    }

    template <typename F, typename... Args>
    TaskIdPtr add(F &&function, Args &&...args)
    {
        using ReturnType = std::invoke_result_t<F, Args...>;
        auto duration = std::chrono::system_clock::now().time_since_epoch();
        auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        TaskIdPtr taskId = std::make_shared<TaskId>(std::hash<decltype(now)>()(now));
        while (findTask(taskId) != nullptr)
        {
            duration = std::chrono::system_clock::now().time_since_epoch();
            now = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
            taskId = std::make_shared<TaskId>(std::hash<decltype(now)>()(now));
        }
        auto task = std::make_shared<Task<F, ReturnType, Args...>>(taskId, std::forward<F>(function), std::forward<Args>(args)...);
        tasks_.enqueue(task);
        return taskId;
    }
    TaskIdPtr add(ITaskPtr taskptr)
    {
        tasks_.enqueue(taskptr);
        return taskptr->getId();
    }
    ExcuteResultPtr execute(TaskIdPtr taskId)
    {
        auto task = findTask(taskId);
        if (task)
        {
            return task->run();
        }
        throw std::runtime_error("Task not found.");
    }
    std::vector<ExcuteResultPtr> executeAll()
    {
        std::vector<ExcuteResultPtr> results;
        auto testTask = tasks_.tryPop();
        while (testTask.has_value())
        {
            {
                results.push_back(testTask.value()->run());
            }
            testTask = tasks_.tryPop();
        }
        return results;
    }
    ExcuteResultPtr execute()
    {
        auto task = tasks_.tryPop();
        if (task.has_value())
        {
            return task.value()->run();
        }
        else
        {
            throw std::runtime_error("Task not found.");
            return nullptr;
        }
    }

    ITaskPtr findTask(TaskIdPtr taskId)
    {
        return tasks_.find([taskId](auto task)
                           { return task->getId()->value() == taskId->value(); });
    }

    bool removeTask(TaskIdPtr taskId)
    {
        return tasks_.removeOne([taskId](const ITaskPtr &task)
                                { return task->getId()->value() == taskId->value(); });
    }

    size_t getTaskCount()
    {
        return tasks_.size();
    }

    void clear()
    {
        tasks_.clear();
    }

private:
    XQueue<ITaskPtr> tasks_;
};

using TaskManagerPtr = std::shared_ptr<TaskManager>;
