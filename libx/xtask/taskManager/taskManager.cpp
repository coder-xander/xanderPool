#include "taskManager.h"

TaskIdPtr TaskManager::add(TaskBasePtr taskptr)
{
    tasks_.enqueue(taskptr);
    return taskptr->getId();
}



ExecuteResultPtr TaskManager::execute()
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


bool TaskManager::removeTask(TaskIdPtr taskId)
{
    return tasks_.removeOne([taskId](const TaskBasePtr &task)
        { return task->getId()->value() == taskId->value(); });
}

size_t TaskManager::getTaskCount()
{
    return tasks_.size();
}

void TaskManager::clear()
{
    tasks_.clear();
}
