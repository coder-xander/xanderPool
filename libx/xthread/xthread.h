#pragma once
#include "../xtask/task/task.h"
#include "../xlock/xlock.h"
#include "../xtask/taskManager/taskManager.h"

class XThread
{
public:
	enum  State
	{
		Running,
		Waitting,
		Exited,

	};
private:
	

	//信号量锁，用户消费、生产task
	XLock tasksXLock_;
	//管理器互斥量
	std::mutex taskManagerMutex_;
	//线程退出标志
	std::atomic_bool exitFlag_;
	//线程
	std::thread thread_;
	//线程的状态
	State status_;
	//获取状态的互斥量
	std::mutex statusMutex_;
	//任务管理器，每个线程都有一个管理器
	TaskManagerPtr taskmanager_;
public:
	explicit XThread();
	~XThread();

public:
	
	static std::shared_ptr<XThread> makeShared()
	{
		return std::make_shared<XThread>();
	}
	bool isWaitting();
	/// @brief 获取状态
	State getState();
	/// @brief 设置状态
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
	int getTaskCount()
	{
		std::lock_guard lock(taskManagerMutex_);
		return  taskmanager_->getTaskCount();

	}
};
using XThreadPtr = std::shared_ptr<XThread>;
