#pragma once
#include "../xtask/xtask.h"

class XThread
{
public:
	explicit XThread()
	{
		taskmanager_ = TaskManager::makeShared();
	}
	~XThread()
	{
	}
	/// @brief 如果可以，线程接受这个任务
	/// @return
	template <typename F, typename... Args>
	bool acceptTask(F &&function, Args &&...args)
	{
		taskmanager_->add(std::forward<F>(function), std::forward<Args>(args)...);
	}

private:
	TaskManagerPtr taskmanager_;
};
