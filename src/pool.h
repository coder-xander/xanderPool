#pragma once
#include <algorithm>
#include <future>
#include <iomanip>
#include <shared_mutex>
#include <mutex>
#include "worker.h"

namespace xander
{
    /// @brief 线程池。即时扩缩容，submit 时决策，无 GC 定时器线程。
    class Pool
    {
    private:
        mutable std::shared_mutex workersMutex_;
        std::vector<WorkerPtr> workers_;

        std::atomic_int workerMinNum_{2};
        std::atomic_int workerMaxNum_{static_cast<int>(std::thread::hardware_concurrency())};

        // 空闲回收阈值（毫秒）
        std::atomic_int idleReclaimMs_{3000};

        // 单例
        inline static std::unique_ptr<Pool> instance_;
        inline static std::mutex instanceMutex_;

    public:
        static Pool* instance()
        {
            if (instance_ == nullptr)
            {
                std::lock_guard<std::mutex> lock(instanceMutex_);
                if (instance_ == nullptr)
                    instance_.reset(new Pool());
            }
            return instance_.get();
        }

        static void singletonReset()
        {
            std::lock_guard<std::mutex> lock(instanceMutex_);
            instance_.reset();
            instance_ = nullptr;
        }

        Pool()
        {
            for (int i = 0; i < workerMinNum_.load(); i++)
                workers_.push_back(Worker::makeShared());
        }

        Pool(int workerMinNum, int workerMaxNum, int idleReclaimMs = 3000)
        {
            workerMinNum_.store(workerMinNum);
            workerMaxNum_.store(workerMaxNum);
            idleReclaimMs_.store(idleReclaimMs);
            for (int i = 0; i < workerMinNum; i++)
                workers_.push_back(Worker::makeShared());
        }

        ~Pool()
        {
            const auto f = asyncDestroyed();
            f.wait();
        }

        std::future<bool> asyncDestroyed()
        {
            return std::async(std::launch::async, [this]()
            {
                std::lock_guard lock(workersMutex_);
                std::vector<std::future<bool>> fs;
                for (auto& w : workers_)
                {
                    fs.push_back(std::async(std::launch::async, [w]() {
                        w->shutdown();
                        return true;
                    }));
                }
                for (auto& f : fs) f.wait();
                workers_.clear();
                return true;
            });
        }

        // ========== 任务提交 ==========

        template <typename F, typename... Args, typename Rt = std::invoke_result_t<F, Args...>>
        TaskResultPtr<Rt> submit(F&& f, Args&&... args)
        {
            auto worker = pickOrCreateWorker();
            return worker->submit(TaskBase::Normal, std::forward<F>(f), std::forward<Args>(args)...);
        }

        template <typename F, typename... Args, typename Rt = std::invoke_result_t<F, Args...>>
        TaskResultPtr<Rt> submit(const TaskBase::Priority& priority, F&& f, Args&&... args)
        {
            auto worker = pickOrCreateWorker();
            return worker->submit(priority, std::forward<F>(f), std::forward<Args>(args)...);
        }

        template <typename F, typename... Args, typename Rt = std::invoke_result_t<F, Args...>>
        TaskResultPtr<Rt> submit(const TaskBase::Priority& priority, TaskPtr<F, Rt, Args...> task)
        {
            auto worker = pickOrCreateWorker();
            return worker->submit(priority, task);
        }

        template <typename F, typename... Args, typename Rt = std::invoke_result_t<F, Args...>>
        TaskResultPtr<Rt> submit(TaskPtr<F, Rt, Args...> task)
        {
            auto worker = pickOrCreateWorker();
            return worker->submit(task);
        }

        void submit(TaskBasePtr task)
        {
            auto worker = pickOrCreateWorker();
            worker->submit(task);
        }

        void submitSome(std::vector<TaskBasePtr> tasks)
        {
            for (auto& t : tasks) submit(t);
        }

        // ========== 配置 ==========

        void setWorkerExpiryTime(int ms) { idleReclaimMs_.store(ms); }

        Pool* useStaticMode(int workerNum = -1)
        {
            if (workerNum == -1)
                workerNum = static_cast<int>(std::thread::hardware_concurrency());
            workerMinNum_.store(workerNum);
            workerMaxNum_.store(workerNum);
            std::lock_guard lock(workersMutex_);
            while (static_cast<int>(workers_.size()) < workerNum)
                workers_.push_back(Worker::makeShared());
            return this;
        }

        // ========== 查询 ==========

        std::vector<TaskBasePtr> findTasks(const std::string& name)
        {
            std::vector<TaskBasePtr> r;
            std::shared_lock lock(workersMutex_);
            for (auto& w : workers_)
            {
                auto tasks = w->findTasks(name);
                r.insert(r.end(), tasks.begin(), tasks.end());
            }
            return r;
        }

        std::vector<WorkerPtr> getIdleWorkers(int num)
        {
            std::shared_lock lock(workersMutex_);
            std::vector<WorkerPtr> idle;
            for (auto& w : workers_)
                if (w->isIdle() && w->taskCount() == 0) idle.push_back(w);
            if (static_cast<int>(idle.size()) >= num) return idle;

            std::vector<WorkerPtr> all(workers_.begin(), workers_.end());
            std::sort(all.begin(), all.end(), [](const WorkerPtr& a, const WorkerPtr& b)
            {
                return a->taskCount() < b->taskCount();
            });
            for (auto& w : all)
            {
                if (static_cast<int>(idle.size()) >= num) break;
                if (std::find(idle.begin(), idle.end(), w) == idle.end())
                    idle.push_back(w);
            }
            return idle;
        }

        WorkerPtr getAnIdleWorker()
        {
            auto idle = getIdleWorkers(1);
            if (idle.empty())
            {
                std::shared_lock lock(workersMutex_);
                return workers_.empty() ? nullptr : workers_.front();
            }
            return idle.front();
        }

        std::string dumpWorkers()
        {
            std::stringstream ss;
            ss << "+-------------------+------+------+------+--------+\n";
            ss << "| Thread ID         | High | Norm | Low  | State  |\n";
            ss << "+-------------------+------+------+------+--------+\n";
            std::shared_lock lock(workersMutex_);
            for (auto& w : workers_)
            {
                const char* s = "?";
                switch (w->state())
                {
                    case Worker::State::Idle:    s = "Idle"; break;
                    case Worker::State::Busy:    s = "Busy"; break;
                    case Worker::State::Stopped: s = "Stop"; break;
                }
                ss << "| " << std::setw(16) << w->idString()
                   << " | " << std::setw(4) << w->highPriorityTaskCount()
                   << " | " << std::setw(4) << w->normalPriorityTaskCount()
                   << " | " << std::setw(4) << w->lowPriorityTaskCount()
                   << " | " << std::setw(6) << s << " |\n";
            }
            ss << "+-------------------+------+------+------+--------+\n";
            return ss.str();
        }

    private:
        /// @brief 核心调度：即时扩缩决策
        WorkerPtr pickOrCreateWorker()
        {
            std::lock_guard lock(workersMutex_);

            reclaimIdleWorkersLocked();

            // 1. 选空闲且队列为空的 worker
            for (auto& w : workers_)
                if (w->isIdle() && w->taskCount() == 0)
                    return w;

            // 2. 未达上限，创建新 worker
            if (static_cast<int>(workers_.size()) < workerMaxNum_.load())
            {
                auto w = Worker::makeShared();
                workers_.push_back(w);
                return w;
            }

            // 3. 已达上限，选任务最少的
            return *std::min_element(workers_.begin(), workers_.end(),
                [](const WorkerPtr& a, const WorkerPtr& b)
                {
                    return a->taskCount() < b->taskCount();
                });
        }

        /// @brief 回收空闲超时的 worker
        void reclaimIdleWorkersLocked()
        {
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
            int threshold = idleReclaimMs_.load();

            auto itr = workers_.begin();
            while (itr != workers_.end())
            {
                if (static_cast<int>(workers_.size()) <= workerMinNum_.load())
                    break;

                auto& w = *itr;
                bool shouldReclaim = false;

                if (w->isIdle() && w->taskCount() == 0)
                {
                    int64_t idleMs = now - w->idleSince();
                    if (idleMs > threshold)
                        shouldReclaim = true;
                }

                if (shouldReclaim)
                {
                    w->shutdown();
                    itr = workers_.erase(itr);
                }
                else
                {
                    ++itr;
                }
            }
        }
    };
}
