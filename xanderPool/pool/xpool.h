#pragma once
#include <future>
#include <shared_mutex>
#include "../worker/worker.h"
namespace xander
{
	///@brief 线程池，所有方法线程安全
	///析构没有完成所有任务的线程池会放弃没有完成的任务。
	class XPool
	{
	private:
		//工作者，
		std::shared_mutex workersMutex_;
		std::vector<WorkerPtr> workers_;
		//标记
		size_t nextWorkerIndex_ = 0;
		std::atomic_int workerMinNum_;
		std::atomic_int workerMaxNum_;
	    inline static std::unique_ptr<XPool> instance_;//单例
		inline static std::mutex instanceMutex_;//单例锁
		int expiryTime_=1000;//空闲的worker的过期时间,单位ms
		std::thread timerThread_;
		std::atomic_bool timerThreadExitFlag_;
	
	private:
		/// @brief 自动管理worker资源,自动管理
		auto startWorkerWatcher()
		{
			timerThread_ = std::thread([this]()
				{
					while (!timerThreadExitFlag_.load())
					{
						std::chrono::milliseconds dura(expiryTime_);
						std::this_thread::sleep_for(dura);
						std::lock_guard lock(workersMutex_);
						auto itr = workers_.begin();
						std::cout<<dumpWorkers()<<"\n";
						while (itr != workers_.end()) {

							// Check if we have more than workerMinNum_ workers remaining
							if (workers_.size() > workerMinNum_)
							{
								auto isbusy = (*itr)->isBusy();
								if (isbusy) {
									++itr;
								}
								else {
									(*itr)->shutdown();
									std::cout << ":remove one worker" << std::endl;
									itr = workers_.erase(itr);

									// if only workerMinNum_ workers are left, break the loop.
									if (workers_.size() <= workerMinNum_) {
										break;
									}
								}
							}
							else
							{
								break;
							}
						}
					}

				});
				
		}
	public:
		//线程安全的单例，自动释放
		static XPool* instance()
		{
			if (instance_ == nullptr)
			{
				std::lock_guard<std::mutex> lock(instanceMutex_);
				if (instance_ == nullptr)
				{
					instance_.reset(new XPool());
				}
			}
			return instance_.get();
		}
		
		auto addAWorker()
		{
			std::cout << "add Worker" << "\n";
			auto w = Worker::makeShared();
			workers_.push_back(w);
			return w;
		}
		XPool()
		{ 
			//最少有两个worker
			workerMinNum_.store(2);
			//获取cpu核心数，创建对应数量线程，最多有处理器核心数量个
			workerMaxNum_.store(std::thread::hardware_concurrency());
			for (size_t i = 0; i < workerMinNum_.load(); i++)
			{
				addAWorker();
			}
			startWorkerWatcher();
		}
		explicit XPool(int workerMinNum, int workerMaxNum)
		{
			workerMinNum_.store(workerMinNum);
			workerMaxNum_.store(workerMaxNum);

			for (size_t i = 0; i < workerMinNum_.load(); i++)
			{
				addAWorker();
			}
		}
		~XPool()
		{
			std::cout << "~XPool" << "\n";
			std::vector<std::future<bool>> fs;
			for (auto e : workers_)
			{
				auto f = std::async([this, e]() {return e->shutdown(); });
				fs.push_back(std::move(f));
			}
			for (auto& f : fs)
			{
				f.get();
			}
			workers_.clear();

		}

		///@brief 线程池的调度1，决定一个线程用于接受一个任务
		WorkerPtr decideAWorkerAverage()
		{

			WorkerPtr selectedThread = workers_[nextWorkerIndex_];
			nextWorkerIndex_ = (nextWorkerIndex_ + 1) % workers_.size();
			return selectedThread;
		}
		///@brief 线程池的调度2，优先使用空闲线程,如果没有空闲线程，就选择任务最少的线程.
		///如果没有空闲线程，并且当前线程数未达到最大值，则创建并返回一个新的线程
		WorkerPtr decideWorkerIdlePriority()
		{
			for (auto worker : workers_)
			{
				if (!worker->isBusy())
				{
					return worker;
				}
			}

			if (workers_.size() < workerMaxNum_.load()) {
				WorkerPtr worker = addAWorker();
				return worker;
			}

			WorkerPtr r = workers_.front();
			for (auto worker : workers_)
			{
				if (worker->getTaskCount() < r->getTaskCount())
				{
					r = worker;
				}
			}

			return r;
		}
		/// @brief 线程池接受一个任务
		///	@param f 任务函数
		///	@param args 任务函数的参数
		///	@return 任务结果
		template <typename F, typename... Args, typename  Rt = std::invoke_result_t < F, Args ...>>
		TaskResultPtr<Rt> submit(F&& f, Args &&...args, const TaskBase::Priority& priority = TaskBase::Normal)
		{
			const auto worker = decideWorkerIdlePriority();
			auto result  =  worker->submit(std::forward<F>(f), std::forward<Args>(args)..., priority);
			return result;
		}
		std::string dumpWorkers()
		{
			std::string s;
			s += "+-------------------+-------------------+\n";
			s += "| Thread ID         | Contained Task Num|\n";
			s += "+-------------------+-------------------+\n";

			for (const auto worker : workers_)
			{
				std::string threadID = "Thread ID: " + worker->idString();
				std::string numTasks = "Contained Task Num: " + std::to_string(worker->getTaskCount());

				const int threadIDSpace = 19 - threadID.length();
				for (int i = 0; i < threadIDSpace; i++)
					threadID += " ";

				const int numTasksSpace = 19 - numTasks.length();
				for (int i = 0; i < numTasksSpace; i++)
					numTasks += " ";

				s += "| " + threadID + " | " + numTasks + " |\n";
				s += "+-------------------+-------------------+\n";
			}

			return s;
		}
	};

}
