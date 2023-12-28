#pragma once
#include "../xlock/xlock.h"
#include "../xtask/task/task.h"
#include "../xtask/taskManager/taskManager.h"
class XThread
{
private:
	std::thread trhead_;
	std::atomic_bool exitFlag_;
	std::condition_variable cv_;
	XLock tasksXLock_;
	std::mutex taskManagerMutex_;

public:
	enum class State
	{
		Running,
		waitting,
		Exited,

	};
	State status_;
	std::mutex statusMutex_;
	State getState()
	{
		std::lock_guard guard(statusMutex_);
		return status_;
	}

	void setStatus(State s)
	{
		std::lock_guard guard(statusMutex_);
		status_ = s;
	}
	explicit XThread() : tasksXLock_(0)
	{
		taskmanager_ = TaskManager::makeShared();
		exitFlag_.store(false);
		trhead_ = std::thread([this]()
							  {
			while (!exitFlag_.load(	))
			{
				setStatus(State::waitting);
				tasksXLock_.acquire();//等待任务
				setStatus(State::Running);
				auto task = taskmanager_->execute(); // 执行队列任务
			} });

		setStatus(State::Exited);
	}
	~XThread()
	{
	}
	/// @brief 如果可以，线程接受这个任务
	/// @return
	template <typename F, typename... Args>
	TaskId acceptTask(F &&function, Args &&...args)
	{
		std::lock_guard guard(taskManagerMutex_);
		auto id = taskmanager_->add(std::forward<F>(function), std::forward<Args>(args)...);
		tasksXLock_.release();
		return id;
	}
	TaskId accceptTask(ITaskPtr taskptr)
	{
		std::lock_guard guard(taskManagerMutex_);
		auto id = taskmanager_->add(std::forward<ITaskPtr>(taskptr));
		tasksXLock_.release();
		return id;
	}

private:
	TaskManagerPtr taskmanager_;
};
using XThreadPtr = std::shared_ptr<XThread>;