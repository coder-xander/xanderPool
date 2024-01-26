#pragma once
#include <format>
#include <future>
#include <iomanip>
#include <shared_mutex>
#include "worker.h"
namespace xander
{
	///@brief thread safe, memory safe. the thread pool ,recommend to use the singleton,if you want to use the singleton,please use the instance() function
	///whenever you deconstruct the XPool,all the task will be abandoned.
	class XPool
	{
	public:
		///@brief constructor
		///setting two workers at least, and the max worker number is the cpu core number, and create two workers
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
		///@brief constructor
		///setting  workerMinNum_ workers at least, and the max worker number is workerMaxNum, and create workerMinNum_ workers
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

		std::future<bool> asyncDestroyed()
		{
			return  std::async(std::launch::async, [this]()
				{
					std::cout << "~XPool" << "\n";
					timerThreadExitFlag_.store(true);
					if (timerThread_.joinable())
					{
						timerThread_.join();
					}
					std::vector<std::future<bool>> fs;
					for (auto e : workers_)
					{
						auto f = std::async([this, e]() {return e->shutdown(); });
						fs.push_back(std::move(f));
					}
					for (auto& f : fs)
					{
						f.wait();
					}
					std::lock_guard lock(workersMutex_);
					workers_.clear();


					return true;
				});


		}

		///@brief deconstruct
		///waiting for every worker to finish the current task,then end the thread,abandon all the task which is not finished
		~XPool()
		{
			auto f = asyncDestroyed();
			f.wait();
		}
	private:

		std::shared_mutex workersMutex_;
		std::vector<WorkerPtr> workers_;//workers
		size_t nextWorkerIndex_ = 0;//flag 
		std::atomic_int workerMinNum_;
		std::atomic_int workerMaxNum_;
		inline static std::unique_ptr<XPool> instance_;//singleton
		inline static std::mutex instanceMutex_;//singleton mutex
		int workerExpiryTime_ = 1000 * 5;//the time of expiry worker,if the worker is not busy for workerExpiryTime_ mills,then the worker will be shutdown
		std::thread timerThread_;//the garbage collection thread
		std::atomic_bool timerThreadExitFlag_;

	private:
		/// @brief manage worker resource .
		///	manage the worker resource,auto recycle the worker which is not busy for workerExpiryTime_ mills, but at least workerMinNum_ workers
		void startWorkersGc()
		{
			timerThread_ = std::thread([this]()
				{
					while (!timerThreadExitFlag_.load())
					{
						std::chrono::milliseconds dura(workerExpiryTime_);
						std::this_thread::sleep_for(dura);
						std::lock_guard lock(workersMutex_);
						printf_s("calculation :\n");
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
		///@brief the thread safe singleton 
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
		/// @brief pool accept a task,decide a worker to accept it
		///	@param f task function type
		///	@param args task function args type
		///	@return task result
		template <typename F, typename... Args, typename  Rt = std::invoke_result_t < F, Args ...>>
		TaskResultPtr<Rt> submit(F&& f, Args &&...args, const TaskBase::Priority& priority = TaskBase::Normal)
		{
			const auto worker = decideWorkerIdlePriority();
			auto result = worker->submit(std::forward<F>(f), std::forward<Args>(args)..., priority);
			return result;
		}
		///@brief add a new worker to the workers container
		///@return WorkerPtr worker just added
		auto addAWorker()
		{
			std::cout << "add Worker" << "\n";
			auto w = Worker::makeShared();
			workers_.push_back(w);
			return w;
		}

		///@brief the policy of decide which worker to accept the next task ,this is the average policy ,all task will be distributed averagely
		WorkerPtr decideAWorkerAverage()
		{

			WorkerPtr selectedThread = workers_[nextWorkerIndex_];
			nextWorkerIndex_ = (nextWorkerIndex_ + 1) % workers_.size();
			return selectedThread;
		}

		///@brief the policy of decide which worker to accept the next task,this is the smarter policy called not busy first policy,if there is no idle worker,then choose the worker which has the least task
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
				if (worker->taskCount() < r->taskCount())
				{
					r = worker;
				}
			}

			return r;
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
		///@brief print all the workers' thread id and the task number they have
		///@return the string will be printed
		std::string dumpWorkers()
		{
			std::stringstream ss;
			ss << "+-------------------+-------------------+-------------------+-------------------+\n";
			ss << "| Thread ID         | Low Priority Num  | Normal Task Num   | High Priority Num |\n";
			ss << "+-------------------+-------------------+-------------------+-------------------+\n";

			for (const auto& worker : workers_)
			{
				auto lowTaskNum = worker->lowPriorityTaskCount();
				auto normalTaskNum = worker->normalPriorityTaskCount();
				auto highTaskNum = worker->highPriorityTaskCount();

				ss << "| " << std::setw(16) << worker->idString();
				ss << " | " << std::setw(16) << lowTaskNum;
				ss << " | " << std::setw(17) << normalTaskNum;
				ss << " | " << std::setw(16) << highTaskNum;
				ss << " |\n";
			}

			ss << "+-------------------+-------------------+-------------------+-------------------+\n";
			return ss.str();
		}

	};
}
