#pragma once
#include <future>
#include <shared_mutex>
#include "../worker/worker.h"
namespace xander
{
	class XPool
	{
	private:
		std::shared_mutex workersMutex_;
		std::vector<WorkerPtr> workers_;
		size_t nextWorkerIndex_ = 0;
	public:
		XPool()
		{
			//获取cpu核心数，创建这么多线程
			auto threadCount = std::thread::hardware_concurrency();
			for (size_t i = 0; i < threadCount; i++)
			{
				workers_.push_back(Worker::makeShared());
			}


		}
		explicit XPool(size_t threadCount)
		{
			for (size_t i = 0; i < threadCount; i++)
			{
				workers_.push_back(Worker::makeShared());
			}
		}
		~XPool()
		{
			std::cout << "~XPool" << std::endl;
			workers_.clear();
		}
		///@brief 增加一个工作的线程
		void addAWorkThread()
		{
			workers_.push_back(Worker::makeShared());
		}
		///@brief 减少一个工作的线程
		void removeAWorkThread()
		{
			//寻找一个空闲的线程，或者任务数量最少的线程
			auto thread = decideWorkerIdlePriority();
			//移除这个线程

			workers_.erase(std::find(workers_.begin(), workers_.end(), thread));
		}

		///@brief 线程池的调度1，决定一个线程用于接受一个任务
		WorkerPtr decideAWorkerAverage()
		{

			WorkerPtr selectedThread = workers_[nextWorkerIndex_];
			nextWorkerIndex_ = (nextWorkerIndex_ + 1) % workers_.size();
			return selectedThread;
		}
		///@brief 线程池的调度2，优先使用空闲线程,如果没有空闲线程，就选择任务最少的线程
		WorkerPtr decideWorkerIdlePriority()
		{

			for (auto worker : workers_)
			{
				if (worker->getState() == Worker::Idle)
				{
					return worker;
				}
			}
			WorkerPtr r = workers_.front();
			for (auto worker : workers_)
			{
				if (worker->getTaskCount() << r->getTaskCount())
				{
					r = worker;
				}
			}
			return r;
		}
		/// @brief 线程池接受一个任务
		template <typename F, typename... Args, typename  Rt = typename  std::invoke_result_t < F, Args ...>>
		TaskResultPtr<Rt> submit(F&& f, Args &&...args)
		{
			auto worker = decideWorkerIdlePriority();
			return  worker->submit(std::forward<F>(f), std::forward<Args>(args)...);
		}
	};

}
