#include "xworker.h"

namespace xander
{
	bool Worker::removeTask(size_t taskId)
	{
		return allTasks_.removeOne([taskId](const TaskBasePtr& task)
			{ return task->getId() == taskId; });
	}



	void Worker::clear()
	{
		allTasks_.clear();
	}

}

