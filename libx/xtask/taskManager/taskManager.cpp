#include "taskManager.h"



bool TaskManager::removeTask(TaskIdPtr taskId)
{
    return tasks_.removeOne([taskId](const TaskBasePtr &task)
        { return task->getId()->value() == taskId->value(); });
}



void TaskManager::clear()
{
    tasks_.clear();
}
