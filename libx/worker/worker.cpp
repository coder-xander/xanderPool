#include "worker.h"


namespace xander
{
	bool Worker::removeTask(size_t taskId)
	{
		return tasks_.removeOne([taskId](const TaskBasePtr& task)
			{ return task->getId() == taskId; });
	}



	void Worker::clear()
	{
		tasks_.clear();
	}

}

