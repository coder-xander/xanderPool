#include "xthread.h"
XThread::State XThread::getState()
{
	std::lock_guard guard(statusMutex_);
	return status_;
}

 void XThread::setStatus(State s)
{
	std::lock_guard guard(statusMutex_);
	status_ = s;
}

XThread::XThread(): tasksXLock_(0)
{
	taskmanager_ = TaskManager::makeShared();
	exitFlag_.store(false);
	thread_ = std::thread([this]()
	{
		while (!exitFlag_.load(	))
		{
			setStatus(State::Waitting);
			tasksXLock_.acquire();//等待任务
			setStatus(State::Running);
			auto task = taskmanager_->execute(); // 执行队列任务
		} });

	setStatus(State::Exited);
}

XThread::~XThread()
{
	std::cout << "~XThread";
	exitFlag_.store(true);
	if (thread_.joinable())
	{
		thread_.join();
	}
	setStatus(State::Exited);
}

TaskIdPtr XThread::acceptTask(TaskBasePtr taskptr)
{
	std::lock_guard guard(taskManagerMutex_);
	auto id = taskmanager_->add(std::forward<TaskBasePtr>(taskptr));
	tasksXLock_.release();
	return id;
}
