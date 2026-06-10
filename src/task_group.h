#pragma once
#include <atomic>
#include <climits>
#include <functional>
#include <future>
#include <memory>
#include <semaphore>
#include <stdexcept>
#include <thread>
#include <vector>

namespace xander
{
    class Pool; // 前向声明

    /// @brief 结构化并发：任务组。
    ///        在作用域内 spawn 多个任务，wait 等待全部完成。
    ///        析构时自动取消未完成的任务。
    ///
    /// 用法：
    ///   {
    ///       TaskGroup group(pool);
    ///       group.spawn([]() { /* task 1 */ });
    ///       group.spawn([]() { /* task 2 */ });
    ///       group.wait(); // 等待全部完成
    ///   } // 析构时自动清理
    class TaskGroup
    {
    public:
        explicit TaskGroup(Pool* pool) : pool_(pool) {}
        ~TaskGroup();

        // 不可复制、不可移动（std::counting_semaphore 不可移动，move 会丢许可）
        TaskGroup(const TaskGroup&) = delete;
        TaskGroup& operator=(const TaskGroup&) = delete;
        TaskGroup(TaskGroup&& other) noexcept = delete;
        TaskGroup& operator=(TaskGroup&& other) noexcept = delete;

        /// @brief 提交一个任务到组中
        template <typename F, typename... Args>
        void spawn(F&& f, Args&&... args)
        {
            if (cancelled_.load())
                return; // 组已取消，忽略新任务

            pending_.fetch_add(1);

            // 包装任务：执行完后递减计数器并释放信号量
            auto wrapper = [this, fn = std::bind(std::forward<F>(f), std::forward<Args>(args)...)]() mutable
            {
                try
                {
                    if (!cancelled_.load())
                    {
                        fn();
                    }
                }
                catch (...)
                {
                    // 捕获异常，存储第一个错误
                    trySetException(std::current_exception());
                }
                pending_.fetch_sub(1);
                doneSem_.release();
            };

            submitToPool(std::move(wrapper));
        }

        /// @brief 等待所有任务完成。阻塞直到组内所有任务结束。
        void wait()
        {
            int expected = pending_.load();
            while (expected > 0)
            {
                doneSem_.acquire();
                expected = pending_.load();
            }

            // 如果有异常，重新抛出
            if (exPtr_)
            {
                std::rethrow_exception(exPtr_);
            }
        }

        /// @brief 尝试等待，超时返回 false
        bool wait_for(int timeoutMs)
        {
            auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(timeoutMs);
            while (pending_.load() > 0)
            {
                if (!tryAcquireUntil(deadline))
                    return false; // 超时
            }
            return true;
        }

        /// @brief 取消所有未开始的任务
        void cancel()
        {
            cancelled_.store(true);
        }

        /// @brief 待完成任务数
        int pendingCount() const { return pending_.load(); }

        /// @brief 是否有错误
        bool hasException() const { return exPtr_ != nullptr; }

    private:
        Pool* pool_{nullptr};
        std::atomic<int> pending_{0};
        std::atomic_bool cancelled_{false};
        std::counting_semaphore<INT_MAX> doneSem_{0};
        std::exception_ptr exPtr_;
        std::mutex exMtx_;

        void submitToPool(std::function<void()> wrapper);

        bool tryAcquireUntil(const std::chrono::steady_clock::time_point& deadline)
        {
            // counting_semaphore 没有 try_acquire_until，用轮询
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (doneSem_.try_acquire())
                    return true;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            return false;
        }

        void trySetException(std::exception_ptr ptr)
        {
            std::lock_guard<std::mutex> lk(exMtx_);
            if (!exPtr_)
                exPtr_ = ptr;
        }
    };
}
