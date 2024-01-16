#pragma once
#include "../xtask/task/task.h"
#include "../xtask/taskManager/taskManager.h"
#include "../xlock/xlock.h"

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
	auto getThreadId()
	{
		return thread_.get_id();
	}

	explicit XThread() : tasksXLock_(0)
	{
		taskmanager_ = TaskManager::makeShared();
		exitFlag_.store(false);
		thread_ = std::thread([this]()
			{
				while (!exitFlag_.load())
				{
					setStatus(State::Waitting);
					//打印状态
					// std::cout << "thread status:" << status_ << std::endl;
					tasksXLock_.acquire();//等待任务
					setStatus(State::Running);
					auto res = taskmanager_->execute();
					std::cout << "use thread id :" << std::this_thread::get_id() << std::endl;
				}
				setStatus(State::Exited);
				//打印
				// std::cout << "thread status:" << status_ << std::endl;

			});

	}
	~XThread()
	{
		// std::cout << "~XThread";
		tasksXLock_.release();
		tasksXLock_.release();
		exitFlag_.store(true);
		if (thread_.joinable())
		{
			thread_.join();
		}
		setStatus(State::Exited);
	}

public:

	static std::shared_ptr<XThread> makeShared()
	{
		return std::make_shared<XThread>();
	}
	bool isWaiting()
	{
		std::lock_guard guard(statusMutex_);
		return status_ == Waitting;
	}
	/// @brief 获取状态
	State getState()
	{
		std::lock_guard guard(statusMutex_);
		return status_;
	}
	/// @brief 设置状态
	void setStatus(State s)
	{
		std::lock_guard guard(statusMutex_);
		status_ = s;
	}

	/// @brief 接受这个任务,用这个线程的任务管理器来处理，可能被任意线程调用。
	/// @return 返回任务结果的封装
	template <typename F, typename... Args>
	TaskResultPtr addTask(F&& f, Args &&...args)
	{

		auto resultPtr = taskmanager_->addTask(std::forward<F>(f), std::forward<Args>(args)...);
		tasksXLock_.release();
		return resultPtr;
	}
	size_t getTaskCount()
	{
		return  taskmanager_->getTaskCount();

	}
};
using XThreadPtr = std::shared_ptr<XThread>;
