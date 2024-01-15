#pragma once
/// @brief 任务管理类

#include <chrono>
#include <future>

#include "../task/task.h"
#include "../../xqueue/xqueue.h"
///@brief 只给一个线程使用，所以不用加锁
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
    TaskResultPtr add(F &&function, Args &&...args)
    {
        using ReturnType = typename  std::invoke_result_t<F, Args...>;
        auto promisePtr = std::make_shared<std::promise<std::any>>();//异步
        auto duration = std::chrono::system_clock::now().time_since_epoch();
        auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        size_t taskId = std::hash<decltype(now)>()(now);
        while (findTask(taskId) != nullptr)
        {
            duration = std::chrono::system_clock::now().time_since_epoch();
            now = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
            taskId = std::hash<decltype(now)>()(now);
        }
        auto task = std::make_shared<Task<F, ReturnType, Args...>>(taskId, std::forward<F>(function), std::forward<Args>(args)...);
        tasks_.enqueue(task);
        TaskResultPtr taskResultPtr = TaskResult::makeShard(taskId,promisePtr);
        task->setTaskResult(taskResultPtr);
        return taskResultPtr;
    }

    std::vector<std::any > executeAll()
    {
        std::vector<std::any > results;
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
    TaskBasePtr nextTask()
    {
        auto task = tasks_.tryPop();
        if (task.has_value())
        {
            return task.value();
        }
        else
        {
            return {};

        }
    }
    TaskBasePtr  execute()
    {
        auto taskOpt = tasks_.tryPop();
        if (taskOpt.has_value())
        {
            auto taskPtr = taskOpt.value()->run();
           
            return   taskPtr;

        }
        return nullptr;
    }
    std::any  execute(size_t taskId)
    {
        auto task = findTask(taskId);
        if (task)
        {
            return task->run(); 
        }
        throw std::runtime_error("Task not found.");
    }
    TaskBasePtr findTask(size_t taskId)
    {
        return tasks_.find([taskId](auto task)
            { return task->getId()== taskId; });
    }
    bool removeTask(size_t taskId);
    size_t getTaskCount(){ return tasks_.size(); }
    void clear();
};
using TaskManagerPtr = std::shared_ptr<TaskManager>;
