#pragma once
/// @brief 任务管理类

#include <chrono>
#include "../task/task.h"
#include "../../xqueue/xqueue.h"
class TaskManager
{
private:
    XQueue<TaskBasePtr> tasks_;
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
    TaskIdPtr add(TaskBasePtr taskptr);
    
    std::vector<ExecuteResultPtr> executeAll();
    ExecuteResultPtr execute();
    ExecuteResultPtr execute(TaskIdPtr taskId)
    {
        auto task = findTask(taskId);
        if (task)
        {
            return task->run();
        }
        throw std::runtime_error("Task not found.");
    }
    TaskBasePtr findTask(TaskIdPtr taskId)
    {
        return tasks_.find([taskId](auto task)
            { return task->getId()->value() == taskId->value(); });
    }
    bool removeTask(TaskIdPtr taskId);
    size_t getTaskCount();
    void clear();
};
using TaskManagerPtr = std::shared_ptr<TaskManager>;
