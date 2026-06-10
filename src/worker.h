#pragma once
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <thread>
#include <vector>
#include "work_stealing.h"
#include "taskResult.h"
#include "task.h"

namespace xander
{
    class Pool; // 前向声明

    /// @brief 工作线程。拥有本地 WorkStealingDeque，
    ///        空闲时从其他 worker 窃取任务。
    class Worker
    {
    public:
        enum class State { Idle, Busy, Stopped };

    private:
        // 本地任务队列（work-stealing deque）
        WorkStealingDeque<TaskBasePtr> localDeque_;

        // 线程
        std::thread thread_;
        std::atomic<State> state_{State::Idle};
        std::atomic_bool exitFlag_{false};

        // CV：本地队列为空时等待，有新任务或退出时唤醒
        std::mutex mtx_;
        std::condition_variable cv_;

        // 指向 Pool（用于窃取其他 worker 的任务）
        Pool* pool_{nullptr};

        std::atomic<int64_t> idleSinceMs_{0};

    public:
        // ========== 性能遥测 ==========
        std::atomic<uint64_t> stealAttempts_{0};   // 尝试从别人那偷的次数
        std::atomic<uint64_t> tasksStolenAway_{0}; // 被其他人偷走的任务数
        std::atomic<uint64_t> idleWaits_{0};       // 进入空闲等待的次数
        std::atomic<uint64_t> tasksExecuted_{0};   // 已执行的任务数

    public:
        static std::shared_ptr<Worker> makeShared(Pool* pool = nullptr)
        {
            return std::make_shared<Worker>(pool);
        }

        explicit Worker(Pool* pool = nullptr) : pool_(pool)
        {
            idleSinceMs_.store(nowMs());
            thread_ = std::thread([this]() { run(); });
        }

        ~Worker()
        {
            shutdown();
        }

        Worker(const Worker&) = delete;
        Worker& operator=(const Worker&) = delete;

        // ========== 任务提交（入队到本地 deque）==========

        template <typename F, typename... Args, typename R = std::invoke_result_t<F, Args...>>
        TaskResultPtr<R> submit(const TaskBase::Priority& priority, F&& function, Args&&... args)
        {
            auto task = makeTask(priority, std::forward<F>(function), std::forward<Args>(args)...);
            enqueue(task);
            return task->getTaskResult();
        }

        template <typename F, typename... Args, typename R = std::invoke_result_t<F, Args...>>
        TaskResultPtr<R> submit(F&& function, Args&&... args)
        {
            auto task = makeTask(TaskBase::Normal, std::forward<F>(function), std::forward<Args>(args)...);
            enqueue(task);
            return task->getTaskResult();
        }

        template <typename F, typename... Args, typename R = std::invoke_result_t<F, Args...>>
        TaskResultPtr<R> submit(TaskPtr<F, R, Args...> task)
        {
            enqueue(task);
            return task->getTaskResult();
        }

        void submit(TaskBasePtr task)
        {
            enqueue(task);
        }

        /// @brief 入队并唤醒 worker
        void enqueue(TaskBasePtr task)
        {
            localDeque_.push(std::move(task));
            cv_.notify_one();
        }

        /// @brief 尝试从本地 deque 取任务
        std::optional<TaskBasePtr> tryPopLocal()
        {
            return localDeque_.pop();
        }

        /// @brief 尝试从本 worker 窃取任务（供其他 worker 调用）
        std::optional<TaskBasePtr> trySteal()
        {
            auto task = localDeque_.steal();
            if (task.has_value())
                tasksStolenAway_.fetch_add(1);
            return task;
        }

        // ========== 生命周期 ==========

        void shutdown()
        {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                exitFlag_.store(true);
            }
            cv_.notify_one();
            if (thread_.joinable())
                thread_.join();
            state_.store(State::Stopped);
        }

        // ========== 状态查询 ==========

        State state() const { return state_.load(); }
        bool isBusy() const { return state_.load() == State::Busy; }
        bool isIdle() const { return state_.load() == State::Idle; }
        bool isStopped() const { return state_.load() == State::Stopped; }
        int64_t idleSince() const { return idleSinceMs_.load(); }
        size_t taskCount() { return localDeque_.size(); }

        std::string idString() const
        {
            std::ostringstream os;
            os << thread_.get_id();
            return os.str();
        }

        void setPool(Pool* pool) { pool_ = pool; }

    private:
        /// @brief 尝试从其他 worker 窃取任务（实现在 pool.h 中）
        std::optional<TaskBasePtr> tryStealFromOthers();

        /// @brief worker 线程主循环
        void run()
        {
            while (true)
            {
                std::optional<TaskBasePtr> task;

                // 1. 先从本地 deque 取（LIFO：最近的优先，缓存友好）
                task = localDeque_.pop();

                // 2. 本地空，尝试从其他 worker 窃取（先检查退出标志避免死锁）
                if (!task.has_value() && !exitFlag_.load())
                {
                    stealAttempts_.fetch_add(1);
                    task = tryStealFromOthers();
                }

                // 3. 有任务就执行
                if (task.has_value())
                {
                    state_.store(State::Busy);
                    task.value()->run();
                    tasksExecuted_.fetch_add(1);
                    state_.store(State::Idle);
                    idleSinceMs_.store(nowMs());
                }
                else
                {
                    // 4. 没有任务可做，等待唤醒
                    state_.store(State::Idle);
                    idleSinceMs_.store(nowMs());
                    idleWaits_.fetch_add(1);
                    std::unique_lock<std::mutex> lk(mtx_);
                    cv_.wait(lk, [this]() {
                        return exitFlag_.load() || !localDeque_.empty();
                    });
                    if (exitFlag_.load())
                        break;
                }
            }
            state_.store(State::Stopped);
        }

        static int64_t nowMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
        }
    };

    using WorkerPtr = std::shared_ptr<Worker>;
}
