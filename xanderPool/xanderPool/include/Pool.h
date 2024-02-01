#pragma once
#include <format>
#include <future>
#include <iomanip>
#include <shared_mutex>
#include "worker.h"
#include <mutex>
namespace xander
{
    ///@brief thread safe, memory safe. the thread pool ,recommend to use the singleton,if you want to use the singleton,please use the instance() function
    ///whenever you deconstruct the Pool,all the task these unfinished will be abandoned.
    class Pool
    {
    private:

        std::shared_mutex workersMutex_;
        std::vector<WorkerPtr> workers_;//workers
        size_t nextWorkerIndex_ = 0;//flag 
        std::atomic_int workerMinNum_;
        std::atomic_int workerMaxNum_;
        inline static std::unique_ptr<Pool> instance_;//singleton
        inline static std::mutex instanceMutex_;//singleton mutex
        int workerExpiryTime_ = 1000 * 5;//the time of expiry worker,if the worker is not busy for workerExpiryTime_ mills,then the worker will be shutdown
        std::thread timerThread_;//the garbage collection thread
        std::atomic_bool timerThreadExitFlag_;
    public:
        ///@brief constructor
        ///setting two workers at least, and the max worker number is the cpu core number, and create two workers
        Pool()
        {
            workerMinNum_.store(2);
            workerMaxNum_.store(std::thread::hardware_concurrency());
            for (size_t i = 0; i < workerMinNum_.load(); i++)
            {
                addAWorker();
            }
            startWorkersGc();
        }
        ///@brief constructor
        ///setting  workerMinNum_ workers at least, and the max worker number is workerMaxNum, and create workerMinNum_ workers
        explicit Pool(int workerMinNum, int workerMaxNum)
        {
            workerMinNum_.store(workerMinNum);
            workerMaxNum_.store(workerMaxNum);

            for (size_t i = 0; i < workerMinNum_.load(); i++)
            {
                addAWorker();
            }
            startWorkersGc();
        }

        ///@brief deconstruct
        ///waiting for every worker to finish the current task,then end the thread,abandon all the task which is not finished
        ~Pool()
        {
            const auto f = asyncDestroyed();
            f.wait();
        }
    private:
        ///@brief async deconstruct all workers and handle something before delete this object
        std::future<bool> asyncDestroyed()
        {
            return  std::async(std::launch::async, [this]()
                {

                    timerThreadExitFlag_.store(true);
                    if (timerThread_.joinable())
                    {
                        timerThread_.join();
                    }
                    std::lock_guard lock(workersMutex_);
                    std::vector<std::future<bool>> fs;
                    for (const auto& e : workers_)
                    {
                        auto f = std::async([this, e]() {return e->shutdown(); });
                        fs.push_back(std::move(f));
                    }

                    for (const auto& f : fs)
                    {
                        f.wait();
                    }
                    workers_.clear();
                    std::cout << "~Pool" << "\n";
                    return true;
                });


        }



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
                        std::unique_lock lock(workersMutex_);
                        printf_s("collecting ... :\n");
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
                        lock.unlock();
                        printf_s(dumpWorkers().data());
                    }
                    printf_s("garbage collection timer thread destroyed . :\n");
                });

        }
    public:
        ///@brief the thread safe singleton 
        ///@return Pool*
        static Pool* instance()
        {
            if (instance_ == nullptr)
            {
                std::lock_guard<std::mutex> lock(instanceMutex_);
                if (instance_ == nullptr)
                {
                    instance_.reset(new Pool());
                }
            }
            return instance_.get();
        }
        /// @brief pool accept a task,decide a worker to accept it,task is default normal priority
        ///	@param f task function type
        ///	@param args task function args type
        ///	@return task result
        template <typename F, typename... Args, typename  Rt = std::invoke_result_t < F, Args ...>>
        TaskResultPtr<Rt> submit(F&& f, Args &&...args)
        {
            const auto worker = decideWorkerByIdlePriorityPolicy();
            const auto result = worker->submit(std::forward<F>(f), std::forward<Args>(args)..., TaskBase::Normal);
            return result;
        }
        /// @brief pool accept a task,decide a worker to accept it,this override provide a param to set priority of task 
        ///	@param f task function type
        ///@param args task function args type
        ///@return task result
        template <typename F, typename... Args, typename  Rt = std::invoke_result_t < F, Args ...>>
        TaskResultPtr<Rt> submit(const TaskBase::Priority& priority, F&& f, Args &&...args)
        {
            const auto worker = decideWorkerByIdlePriorityPolicy();
            const auto result = worker->submit(std::forward<F>(f), std::forward<Args>(args)..., priority);
            return result;
        }
        /// @brief pool accept a task,the task is your made previously.so make anytime,and submit anytime
        ///	@param f task function type
        ///	@param args task function args type
        ///	@return task result
        template <typename F, typename... Args, typename  Rt = std::invoke_result_t < F, Args ...>>
        TaskResultPtr<Rt> submit(const TaskBase::Priority& priority, TaskPtr<F, Rt, Args...> task)
        {
            const auto worker = decideWorkerByIdlePriorityPolicy();
            return worker->submit(task, priority);
        }
        /// @brief pool accept a task,the task is your made previously.so make anytime,and submit anytime
        ///	@param f task function type
        ///	@param args task function args type
        ///	@return task result
        template <typename F, typename... Args, typename  Rt = std::invoke_result_t < F, Args ...>>
        TaskResultPtr<Rt> submit(TaskPtr<F, Rt, Args...> task)
        {
            const auto worker = decideWorkerByIdlePriorityPolicy();
            return worker->submit(task, TaskBase::Normal);
        }
        void submit(TaskBasePtr task)
        {
            const auto worker = decideWorkerByIdlePriorityPolicy();
            return worker->submit(task, TaskBase::Normal);
        }
        /// @brief pool accept some tasks,this function have no return value,you can use origin task (you just given) instance to get result.
        ///	@param f task function type
        ///	@param args task function args type
        ///	@return task result
        void  submitSome(std::vector<TaskBasePtr> tasks)
        {
            for (auto e : tasks)
            {
                submit(e);
            }
        }
        ///@brief add a new worker to the workers container
        ///@return WorkerPtr worker just added
        auto addAWorker()
        {
            std::cout << "add Worker" << "\n";
            const auto w = Worker::makeShared();
            workers_.push_back(w);
            return w;
        }

        ///@brief The policy of deciding which workers to assign the next task to follows an average policy, wherein all tasks are distributed to the workers evenly.
        WorkerPtr decideAWorkerByAveragePolicy()
        {

            WorkerPtr selectedThread = workers_[nextWorkerIndex_];
            nextWorkerIndex_ = (nextWorkerIndex_ + 1) % workers_.size();
            return selectedThread;
        }

        ///@brief The policy that decides which worker accepts the next task is a smarter one, known as the 'Not Busy First' policy. If there are no idle workers, then the policy will select the worker who has the fewest tasks.
        WorkerPtr decideWorkerByIdlePriorityPolicy()
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

        ///@brief get num workers which is not busy.if all workers is busy,return workers which has the fewest tasks
        std::vector<WorkerPtr> getIdleWorkers(int num)
        {
            std::lock_guard lock(workersMutex_);
            std::vector<WorkerPtr> r;
            for (auto worker : workers_)
            {
                if (!worker->isBusy())
                {
                    r.push_back(worker);
                }
            }
            if (r.size() < num)
            {
                for (auto worker : workers_)
                {
                    if (worker->taskCount() < r.front()->taskCount())
                    {
                        r.push_back(worker);
                    }
                }
            }
            return r;
        }
        WorkerPtr getAnIdleWorker()
        {
            return getIdleWorkers(1).front();
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
            std::lock_guard lock(workersMutex_);
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
