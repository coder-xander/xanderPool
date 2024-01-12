#pragma once
#include "../xtask/task/task.h"
#include "../xlock/xlock.h"
#include "../xtask/taskManager/taskManager.h"

class XThread
{
private:
	enum  State
	{
		Running,
		Waitting,
		Exited,

	};
	std::condition_variable cv_;
	XLock tasksXLock_;
	std::mutex taskManagerMutex_;
	std::atomic_bool exitFlag_;
	std::thread thread_;
	State status_;
	std::mutex statusMutex_;
	TaskManagerPtr taskmanager_;
public:
	explicit XThread();
	~XThread();

public:

	static std::shared_ptr<XThread> makeShared()
	{
		return std::make_shared<XThread>();
	}
	State getState();
	void setStatus(State s);
	/// @brief 接受这个任务,用这个线程的任务管理器来处理
	/// @return 返回任务id
	TaskIdPtr acceptTask(TaskBasePtr taskptr);
	/// @brief 接受这个任务,用这个线程的任务管理器来处理
	/// @return 返回任务id
	template <typename F, typename... Args>
	TaskId acceptTask(F &&function, Args &&...args)
	{
		std::lock_guard guard(taskManagerMutex_);
		auto id = taskmanager_->add(std::forward<F>(function), std::forward<Args>(args)...);
		tasksXLock_.release();
		return id;
	}

};
using XThreadPtr = std::shared_ptr<XThread>;
