#pragma once
#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

namespace xander
{
    /// @brief 工作窃取双端队列。
    /// Owner 端（front）：push/pop，LIFO 顺序，利用局部性。
    /// Stealer 端（back）：steal，FIFO 顺序，公平分配。
    /// 内部用 deque 实现，mutex 保护。
    template <typename T>
    class WorkStealingDeque
    {
    public:
        /// @brief 从 owner 端推入任务
        void push(T item)
        {
            std::lock_guard<std::mutex> lk(mtx_);
            deque_.push_front(std::move(item));
        }

        /// @brief 从 owner 端弹出任务（LIFO：最近推入的先出）
        std::optional<T> pop()
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (deque_.empty()) return std::nullopt;
            T item = std::move(deque_.front());
            deque_.pop_front();
            return item;
        }

        /// @brief 从 stealer 端窃取任务（FIFO：最早推入的先出）
        std::optional<T> steal()
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (deque_.empty()) return std::nullopt;
            T item = std::move(deque_.back());
            deque_.pop_back();
            return item;
        }

        bool empty() const
        {
            std::lock_guard<std::mutex> lk(mtx_);
            return deque_.empty();
        }

        size_t size() const
        {
            std::lock_guard<std::mutex> lk(mtx_);
            return deque_.size();
        }

        void clear()
        {
            std::lock_guard<std::mutex> lk(mtx_);
            deque_.clear();
        }

    private:
        std::deque<T> deque_;
        mutable std::mutex mtx_;
    };
}
