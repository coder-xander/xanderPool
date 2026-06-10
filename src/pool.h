#pragma once
#include <algorithm>
#include <atomic>
#include <future>
#include <iomanip>
#include <mutex>
#include <random>
#include <shared_mutex>
#include "worker.h"
#include "task_group.h"

namespace xander
{
    /// @brief 线程池。支持 work-stealing 和结构化并发。
    class Pool
    {
    private:
        mutable std::shared_mutex workersMutex_;
        std::vector<WorkerPtr> workers_;

        std::atomic_int workerMinNum_{2};
        std::atomic_int workerMaxNum_{static_cast<int>(std::thread::hardware_concurrency())};
        std::atomic_int idleReclaimMs_{3000};

        // 全局任务计数器（用于 steal 决策）
        std::atomic<size_t> totalTasks_{0};

        // 单例
        inline static std::unique_ptr<Pool> instance_;
        inline static std::mutex instanceMutex_;

        // 用于随机选择 steal 目标
        std::mt19937 rng_{std::random_device{}()};

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
                workers_.push_back(Worker::makeShared(this));
        }

        Pool(int workerMinNum, int workerMaxNum, int idleReclaimMs = 3000)
        {
            workerMinNum_.store(workerMinNum);
            workerMaxNum_.store(workerMaxNum);
            idleReclaimMs_.store(idleReclaimMs);
            for (int i = 0; i < workerMinNum; i++)
                workers_.push_back(Worker::makeShared(this));
        }

        ~Pool()
        {
            const auto f = asyncDestroyed();
            f.wait();
        }

        std::future<bool> asyncDestroyed()
        {
            // 先拷贝 worker 列表，再释放锁，避免与 steal 死锁
            std::vector<WorkerPtr> workersCopy;
            {
                std::lock_guard lock(workersMutex_);
                workersCopy = workers_;
                workers_.clear();
            }
            return std::async(std::launch::async, [workersCopy = std::move(workersCopy)]() mutable
            {
                std::vector<std::future<bool>> fs;
                for (auto& w : workersCopy)
                {
                    fs.push_back(std::async(std::launch::async, [w]() {
                        w->shutdown();
                        return true;
                    }));
                }
                for (auto& f : fs) f.wait();
                return true;
            });
        }

        // ========== 任务提交 ==========

        template <typename F, typename... Args, typename Rt = std::invoke_result_t<F, Args...>>
        TaskResultPtr<Rt> submit(F&& f, Args&&... args)
        {
            auto worker = pickWorker();
            auto result = worker->submit(TaskBase::Normal, std::forward<F>(f), std::forward<Args>(args)...);
            totalTasks_.fetch_add(1);
            return result;
        }

        template <typename F, typename... Args, typename Rt = std::invoke_result_t<F, Args...>>
        TaskResultPtr<Rt> submit(const TaskBase::Priority& priority, F&& f, Args&&... args)
        {
            auto worker = pickWorker();
            auto result = worker->submit(priority, std::forward<F>(f), std::forward<Args>(args)...);
            totalTasks_.fetch_add(1);
            return result;
        }

        template <typename F, typename... Args, typename Rt = std::invoke_result_t<F, Args...>>
        TaskResultPtr<Rt> submit(const TaskBase::Priority& priority, TaskPtr<F, Rt, Args...> task)
        {
            auto worker = pickWorker();
            auto result = worker->submit(priority, task);
            totalTasks_.fetch_add(1);
            return result;
        }

        template <typename F, typename... Args, typename Rt = std::invoke_result_t<F, Args...>>
        TaskResultPtr<Rt> submit(TaskPtr<F, Rt, Args...> task)
        {
            auto worker = pickWorker();
            auto result = worker->submit(task);
            totalTasks_.fetch_add(1);
            return result;
        }

        void submit(TaskBasePtr task)
        {
            auto worker = pickWorker();
            worker->submit(task);
            totalTasks_.fetch_add(1);
        }

        void submitSome(std::vector<TaskBasePtr> tasks)
        {
            for (auto& t : tasks) submit(t);
        }

        // ========== 结构化并发 ==========

        /// @brief 创建一个任务组
        TaskGroup createGroup()
        {
            return TaskGroup(this);
        }

        // ========== Work-Stealing 接口（供 Worker 调用）==========

        /// @brief 从其他 worker 窃取任务
        std::optional<TaskBasePtr> stealFromRandomWorker(Worker* thief)
        {
            std::shared_lock lock(workersMutex_);
            if (workers_.size() <= 1) return std::nullopt;

            // 随机选择一个 worker（跳过自己）
            std::uniform_int_distribution<size_t> dist(0, workers_.size() - 1);
            size_t startIdx = dist(rng_);

            for (size_t i = 0; i < workers_.size(); ++i)
            {
                size_t idx = (startIdx + i) % workers_.size();
                auto& w = workers_[idx];
                if (w.get() != thief)
                {
                    auto task = w->trySteal();
                    if (task.has_value())
                        return task;
                }
            }
            return std::nullopt;
        }

        /// @brief 内部提交（TaskGroup 使用，不增加 totalTasks 计数）
        void submitInternal(std::function<void()> wrapper)
        {
            auto worker = pickWorker();
            auto task = makeTask(TaskBase::low, std::move(wrapper));
            worker->submit(task);
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
            {
                auto w = Worker::makeShared(this);
                workers_.push_back(w);
            }
            return this;
        }

        // ========== 查询 ==========

        size_t totalTaskCount() const { return totalTasks_.load(); }

        std::vector<TaskBasePtr> findTasks(const std::string& name)
        {
            // work-stealing 模式下任务在 worker 本地 deque 中
            // findTasks 需要遍历所有 worker 的 deque
            std::vector<TaskBasePtr> r;
            std::shared_lock lock(workersMutex_);
            // 注意：WorkStealingDeque 没有提供遍历接口
            // findTasks 在 work-stealing 模式下受限
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
            ss << "+-------------------+------+--------+-------+--------+------+------+\n";
            ss << "| Thread ID         | Tasks| State  | Steals| Stolen | Idle | Exec |\n";
            ss << "+-------------------+------+--------+-------+--------+------+------+\n";
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
                   << " | " << std::setw(4) << w->taskCount()
                   << " | " << std::setw(6) << s
                   << " | " << std::setw(5) << w->stealAttempts_.load()
                   << " | " << std::setw(6) << w->tasksStolenAway_.load()
                   << " | " << std::setw(4) << w->idleWaits_.load()
                   << " | " << std::setw(4) << w->tasksExecuted_.load()
                   << " |\n";
            }
            ss << "+-------------------+------+--------+-------+--------+------+------+\n";
            return ss.str();
        }

    private:
        /// @brief 选择 worker：空闲优先 → 创建新的 → 任务最少
        WorkerPtr pickWorker()
        {
            std::vector<WorkerPtr> toReclaim;
            WorkerPtr picked;

            {
                std::lock_guard lock(workersMutex_);

                // 回收空闲 worker（仅从列表移除，shutdown 在锁外执行，避免死锁）
                reclaimIdleWorkersLocked(toReclaim);

                // 1. 空闲且队列为空的 worker
                for (auto& w : workers_)
                    if (w->isIdle() && w->taskCount() == 0)
                    {
                        picked = w;
                        break;
                    }

                // 2. 未达上限，创建新 worker
                if (!picked && static_cast<int>(workers_.size()) < workerMaxNum_.load())
                {
                    picked = Worker::makeShared(this);
                    workers_.push_back(picked);
                }

                // 3. 已达上限，选任务最少的
                if (!picked)
                {
                    picked = *std::min_element(workers_.begin(), workers_.end(),
                        [](const WorkerPtr& a, const WorkerPtr& b)
                        {
                            return a->taskCount() < b->taskCount();
                        });
                }
            }

            // 锁已释放：安全 shutdown 被回收的 worker（不会与 stealing 死锁）
            for (auto& w : toReclaim)
                w->shutdown();

            return picked;
        }

        /// @brief 识别并移除过久空闲的 worker，存入 toReclaim
        ///         调用方必须在锁外执行 shutdown
        void reclaimIdleWorkersLocked(std::vector<WorkerPtr>& toReclaim)
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
                if (w->isIdle() && w->taskCount() == 0 && (now - w->idleSince()) > threshold)
                {
                    toReclaim.push_back(std::move(w));
                    itr = workers_.erase(itr);
                }
                else
                {
                    ++itr;
                }
            }
        }

        // TaskGroup 需要访问 submitInternal
        friend class TaskGroup;
    };

    // ========== Worker::tryStealFromOthers 实现（需要 Pool 完整定义）==========

    inline std::optional<TaskBasePtr> Worker::tryStealFromOthers()
    {
        if (pool_)
            return pool_->stealFromRandomWorker(this);
        return std::nullopt;
    }

    // ========== TaskGroup 实现 ==========

    // TaskGroup constructor is now inline in task_group.h

    inline TaskGroup::~TaskGroup()
    {
        cancel();
        // 等待所有已提交的任务完成（任务 wrapper 负责递减 pending_）
        while (pending_.load() > 0)
        {
            doneSem_.acquire();
        }
    }

    inline void TaskGroup::submitToPool(std::function<void()> wrapper)
    {
        pool_->submitInternal(std::move(wrapper));
    }
}
