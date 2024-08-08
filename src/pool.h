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
        std::vector<WorkerPtr> workers_; //workers
        size_t nextWorkerIndex_ = 0; //flag
        std::atomic_int workerMinNum_;
        std::atomic_int workerMaxNum_;
        inline static std::unique_ptr<Pool> instance_; //singleton
        inline static std::mutex instanceMutex_; //singleton mutex
        std::atomic_int workerExpiryTime_;
        //the time of expiry worker,if the worker is not busy for workerExpiryTime_ mills,then the worker will be shutdown
        std::thread timerThread_; //the garbage collection thread
        std::atomic_bool timerThreadExitFlag_;

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

        ///@brief singleton resetting
        static void singletonReset()
        {
            std::lock_guard<std::mutex> lock(instanceMutex_);
            instance_.reset();
            instance_ = nullptr;
        }

        ///@bref find tasks by name
        std::vector<TaskBasePtr> findTasks(const std::string& name)
        {
            std::vector<TaskBasePtr> r;
            std::lock_guard lock(workersMutex_);
            for (const auto& worker : workers_)
            {
                auto tasks = worker->findTasks(name);
                for (const auto& task : tasks)
                {
                    r.push_back(task);
                }
            }
            return r;
        }


        //use static mode to create a static  pool、you can set the worker number，if the worker number is -1,then the worker number is the cpu core number
        Pool* useStaticMode(int workerNum = -1)
        {
            if (workerNum == -1)
            {
                auto maxValue = std::thread::hardware_concurrency();
                workerMaxNum_ = maxValue;
                workerMinNum_ = maxValue;
            }
            else
            {
                workerMaxNum_ = workerNum;
                workerMinNum_ = workerNum;
            }

            return instance_.get();
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

        ///@brief constructor
        ///setting two workers at least, and the max worker number is the cpu core number, and create two workers
        explicit Pool()
        {
            workerMinNum_.store(2);
            workerMaxNum_.store(std::thread::hardware_concurrency());
            workerExpiryTime_.store(5000);
            for (size_t i = 0; i < workerMinNum_.load(); i++)
            {
                addAWorker();
            }
            startWorkersGc();
        }

        void setWorkerExpiryTime(int workerExpiryTime)
        {
            workerExpiryTime_.store(workerExpiryTime);
        }

        ///@brief constructor
        ///setting  workerMinNum_ workers at least, and the max worker number is workerMaxNum, and create workerMinNum_ workers
        explicit Pool(int workerMinNum, int workerMaxNum, int workerExpiryTime = 5000)
        {
            workerMinNum_.store(workerMinNum);
            workerMaxNum_.store(workerMaxNum);
            workerExpiryTime_.store(workerExpiryTime);
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


        ///@brief async deconstruct all workers and handle something before delete this object
        std::future<bool> asyncDestroyed()
        {
            return std::async(std::launch::async, [this]()
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
                    auto f = std::async([e]() { return e->shutdown(); });
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

        //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        //------------------Override 1, submit F,R,Args... -----------------------
        ///@brief The pool receives a task and delegates it to a worker based on idle priority policy; tasks are normal priority by default.
        ///@param f Task function type
        ///@param args Task function argument types
        ///@return Task result
        template <typename F, typename... Args, typename Rt = std::invoke_result_t<F, Args...>>
        TaskResultPtr<Rt> submit(F&& f, Args&&... args)
        {
            const auto worker = decideWorkerByIdlePriorityPolicy();
            const auto result = worker->submit(TaskBase::Normal, std::forward<F>(f), std::forward<Args>(args)...);
            return result;
        }

        //------------------Override 2, submit priority task ---------------
        ///@brief The pool receives a task and delegates it to a worker based on idle priority policy; this override allows specifying task priority.
        ///@param f Task function type
        ///@param args Task function argument types
        ///@return Task result
        template <typename F, typename... Args, typename Rt = std::invoke_result_t<F, Args...>>
        TaskResultPtr<Rt> submit(const TaskBase::Priority& priority, F&& f, Args&&... args)
        {
            const auto worker = decideWorkerByIdlePriorityPolicy();
            const auto result = worker->submit(priority, std::forward<F>(f), std::forward<Args>(args)...);
            return result;
        }

        //------------------Override 3, submit TaskPtr------------------
        ///@brief The pool accepts a task that you have previously created. Submit it anytime.
        ///@param f Task function type
        ///@param args Task function argument types
        ///@return Task result
        template <typename F, typename... Args, typename Rt = std::invoke_result_t<F, Args...>>
        TaskResultPtr<Rt> submit(const TaskBase::Priority& priority, TaskPtr<F, Rt, Args...> task)
        {
            const auto worker = decideWorkerByIdlePriorityPolicy();
            return worker->submit(priority, task);
        }

        //------------------Override 4, submit TaskPtr without priority -----------
        ///@brief The pool accepts a task that you have previously created. Submit it anytime.
        ///@param f Task function type
        ///@param args Task function argument types
        ///@return Task result
        template <typename F, typename... Args, typename Rt = std::invoke_result_t<F, Args...>>
        TaskResultPtr<Rt> submit(TaskPtr<F, Rt, Args...> task)
        {
            const auto worker = decideWorkerByIdlePriorityPolicy();
            return worker->submit(task);
        }

        //----------------------------------override 3,submit TaskBasePtr (submit some)---------------------------------------------------------------------------------------------
        ///@brief pool accept one task,this function have no return value,you can use origin task (you just given) instance to get result.
        void submit(TaskBasePtr task)
        {
            const auto worker = decideWorkerByIdlePriorityPolicy();
            return worker->submit(task);
        }

        /// @brief pool accept some tasks,this function have no return value,you can use origin task (you just given) instance to get result.
        ///	@param f task function type
        ///	@param args task function args type
        ///	@return task result
        void submitSome(std::vector<TaskBasePtr> tasks)
        {
            for (auto e : tasks)
            {
                submit(e);
            }
        }

        //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    private:
        /// @brief manage worker resource .
      ///	manage the worker resource,auto recycle the worker which is not busy for workerExpiryTime_ mills, but at least workerMinNum_ workers
        void startWorkersGc()
        {
            timerThread_ = std::thread([this]()
            {
                while (!timerThreadExitFlag_.load())
                {
                    std::chrono::milliseconds dura(workerExpiryTime_.load());
                    std::this_thread::sleep_for(dura);
                    std::unique_lock lock(workersMutex_);
                    std::cout << "collecting ... " << std::endl;
                    auto itr = workers_.begin();
                    while (itr != workers_.end())
                    {
                        if (workers_.size() > workerMinNum_)
                        {
                            auto isbusy = (*itr)->isBusy();
                            if (isbusy)
                            {
                                ++itr;
                            }
                            else
                            {
                                (*itr)->shutdown();
                                itr = workers_.erase(itr);
                                if (workers_.size() <= workerMinNum_)
                                {
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
                    std::cout << dumpWorkers() << std::endl;
                }
                std::cout << "garbage collection timer thread destroyed . :\n";
            });
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

            if (workers_.size() < workerMaxNum_.load())
            {
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


        ///@brief The policy of deciding which workers to assign the next task to follows an average policy, wherein all tasks are distributed to the workers evenly.
        WorkerPtr decideAWorkerByAveragePolicy()
        {
            WorkerPtr selectedThread = workers_[nextWorkerIndex_];
            nextWorkerIndex_ = (nextWorkerIndex_ + 1) % workers_.size();
            return selectedThread;
        }

    public:
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

        ///@brief get a worker which is not busy,if all workers is busy,return workers which has the fewest tasks
        WorkerPtr getAnIdleWorker()
        {
            return getIdleWorkers(1).front();
        }

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
