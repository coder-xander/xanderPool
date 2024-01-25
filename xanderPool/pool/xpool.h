#pragma once
#include <format>
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
		int workerExpiryTime_=1000*5;//空闲的worker的过期时间,单位ms
		std::thread timerThread_;
		std::atomic_bool timerThreadExitFlag_;

	private:
		/// @brief 管理worker资源,自动回收超时的worker，但是会保留至少两个workerMinNum_个worker
		void startWorkersGc()
		{
			timerThread_ = std::thread([this]()
				{
					while (!timerThreadExitFlag_.load())
					{
						std::chrono::milliseconds dura(workerExpiryTime_);
						std::this_thread::sleep_for(dura);
						std::lock_guard lock(workersMutex_);
						printf_s("cleaning ...............\n");
						auto itr = workers_.begin();
						while (itr != workers_.end()) {
							if (workers_.size() > workerMinNum_)
							{
								auto isbusy = (*itr)->isBusy();
								if (isbusy) {
									++itr;
								}
								else {
									(*itr)->shutdown();
									itr = workers_.erase(itr);
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
						printf_s(dumpWorkers().data());
					}

				});
				
		}
	public:
		///@brief 线程安全的单例，自动释放
		///@return XPool*
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
		///@brief 添加一个工作者
		///@return WorkerPtr 被添加的新的worker
		auto addAWorker()
		{
			std::cout << "add Worker" << "\n";
			auto w = Worker::makeShared();
			workers_.push_back(w);
			return w;
		}
		///@brief 构造函数
		///设置最少有两个worker，最多为cpu核心数量个worker，并创建两个worker
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
			startWorkersGc();
		}
		///@brief 构造函数
		///设置最少有workerMinNum个worker，最多为workerMaxNum个worker，并创建workerMinNum个worker
		explicit XPool(int workerMinNum, int workerMaxNum)
		{
			workerMinNum_.store(workerMinNum);
			workerMaxNum_.store(workerMaxNum);

			for (size_t i = 0; i < workerMinNum_.load(); i++)
			{
				addAWorker();
			}
			startWorkersGc();
		}
		///@brief 析构函数，等待每个worker结束当前的任务，然后结束线程，丢弃没有完成的所有任务
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
			std::lock_guard lock(workersMutex_);
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
		//打包一组任务为一个任务
		// template <typename F, typename... Args, typename  Rt = std::invoke_result_t < F, Args ...>>
		// TaskResultPtr<Rt> submitGroupAsOne(std::vector<std::pair<F, std::tuple<Args...>>> functions, const TaskBase::Priority& priority = TaskBase::Normal)
		// {
		// 	return submit([functions]() {
		// 		for (auto& func : functions)
		// 		{
		// 			std::apply(std::get<0>(func), std::get<1>(func));
		// 		}
		// 		}, priority);
		// }
		///@brief 打印所有的wokers的线程id和他们现在拥有的任务的数量
		///@return 字符串
		std::string dumpWorkers()
		{
			std::string s;
			s += "+-------------------+-------------------+\n";
			s += "| Thread ID         | Contained Task Num|\n";
			s += "+-------------------+-------------------+\n";

			for (const auto worker : workers_)
			{
				std::string threadID = "Thread ID: " + worker->idString();
				auto taskNum = worker->getTaskCount();
				std::string numTasks = "Contained Task Num: " + std::to_string(taskNum);

				const int threadIDSpace =  threadID.length();
				for (int i = 0; i < threadIDSpace; i++)
					threadID += " ";

				const int numTasksSpace = numTasks.length();
				for (int i = 0; i < numTasksSpace; i++)
					numTasks += " ";

				s += "| " + threadID + " | " + numTasks + " |\n";
				s += "+-------------------+-------------------+\n";
			}

			return s;
		}
	};

}
