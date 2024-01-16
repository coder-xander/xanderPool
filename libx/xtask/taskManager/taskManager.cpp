#include "taskManager.h"



bool TaskManager::removeTask(size_t taskId)
{
    return tasks_.removeOne([taskId](const TaskBasePtr& task)
        { return task->getId() == taskId; });
}



void TaskManager::clear()
{
    tasks_.clear();
}
