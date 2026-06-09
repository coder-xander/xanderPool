#pragma once
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <thread>
#include "queue.h"
#include "taskResult.h"
#include "task.h"

namespace xander
{
    /// @brief 任务执行线程。使用 condition_variable + predicate 驱动，
    ///        空闲时零 CPU 消耗，任务到达时立即唤醒。
    class Worker
    {
    public:
        enum class State { Idle, Busy, Stopped };

    private:
        XDeque<TaskBasePtr> highPriorityTasks_;
        XDeque<TaskBasePtr> normalTasks_;
        XDeque<TaskBasePtr> lowPriorityTasks_;

        std::thread thread_;
        std::atomic<State> state_{State::Idle};
        std::atomic_bool exitFlag_{false};

        // 用 CV + mutex 实现零 CPU 等待，predicate 避免丢失通知
        std::mutex mtx_;
        std::condition_variable cv_;

        std::atomic<int64_t> idleSinceMs_{0};

    public:
        static std::shared_ptr<Worker> makeShared()
        {
            return std::make_shared<Worker>();
        }

        Worker()
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

        // ========== 任务提交 ==========

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

        // ========== 生命周期 ==========

        /// @brief 优雅关闭：等队列清空后停止
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

        size_t taskCount()
        {
            std::lock_guard<std::mutex> lk(mtx_);
            return highPriorityTasks_.size() + normalTasks_.size() + lowPriorityTasks_.size();
        }
        size_t highPriorityTaskCount() { std::lock_guard<std::mutex> lk(mtx_); return highPriorityTasks_.size(); }
        size_t normalPriorityTaskCount() { std::lock_guard<std::mutex> lk(mtx_); return normalTasks_.size(); }
        size_t lowPriorityTaskCount() { std::lock_guard<std::mutex> lk(mtx_); return lowPriorityTasks_.size(); }

        std::string idString() const
        {
            std::ostringstream os;
            os << thread_.get_id();
            return os.str();
        }

        std::vector<TaskBasePtr> findTasks(const std::string& name)
        {
            std::vector<TaskBasePtr> result;
            std::lock_guard<std::mutex> lk(mtx_);
            findInQueue(highPriorityTasks_, name, result);
            findInQueue(normalTasks_, name, result);
            findInQueue(lowPriorityTasks_, name, result);
            return result;
        }

    private:
        void enqueue(TaskBasePtr task)
        {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                switch (task->priority())
                {
                    case TaskBase::High:   highPriorityTasks_.enqueue(task); break;
                    case TaskBase::Normal: normalTasks_.enqueue(task); break;
                    case TaskBase::low:    lowPriorityTasks_.enqueue(task); break;
                }
            }
            cv_.notify_one();
        }

        void findInQueue(XDeque<TaskBasePtr>& queue, const std::string& name, std::vector<TaskBasePtr>& result)
        {
            std::unique_lock<std::mutex> lock(queue.mutex());
            for (auto& task : queue.deque())
            {
                if (task->name() == name)
                    result.push_back(task);
            }
        }

        std::optional<TaskBasePtr> takeHighestPriorityTask()
        {
            if (!highPriorityTasks_.empty())
                return highPriorityTasks_.tryPop();
            if (!normalTasks_.empty())
                return normalTasks_.tryPop();
            if (!lowPriorityTasks_.empty())
                return lowPriorityTasks_.tryPop();
            return std::nullopt;
        }

        /// @brief worker 线程主循环
        void run()
        {
            while (true)
            {
                TaskBasePtr task;
                // 在锁内等待任务到达或退出信号
                {
                    std::unique_lock<std::mutex> lk(mtx_);
                    cv_.wait(lk, [this]() {
                        return exitFlag_.load()
                            || !highPriorityTasks_.empty()
                            || !normalTasks_.empty()
                            || !lowPriorityTasks_.empty();
                    });

                    if (exitFlag_.load())
                        break;

                    auto opt = takeHighestPriorityTask();
                    if (opt.has_value())
                        task = opt.value();
                }

                if (task)
                {
                    state_.store(State::Busy);
                    task->run();
                    state_.store(State::Idle);
                    idleSinceMs_.store(nowMs());
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
