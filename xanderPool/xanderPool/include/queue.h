#pragma once
#include <deque>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <optional>
namespace xander {

	///@brief thread safe deque,all functions of this was thread safe
	template <typename T>
	class XDeque
	{
	private:
		std::deque<T> deque_;
		mutable std::mutex mutex_;
		std::condition_variable condVar_;
	public:
		auto& deque() { return deque_; };
		auto& mutex() const { return mutex_; };
	public:

		XDeque() = default;
		XDeque(const XDeque& other)
		{
			std::lock_guard<std::mutex> lock(other.mutex_);
			deque_ = other.deque_;
		}
		~XDeque() = default;
		///@brief push back the value to the deque
		void enqueue(const T& value)
		{
			std::lock_guard<std::mutex> lock(mutex_);
			deque_.push_back(std::move(value));
			condVar_.notify_one();
		}
		std::optional<T> tryPop()
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (deque_.empty())
			{
				return std::nullopt;
			}
			auto value = std::move(deque_.front());
			deque_.pop_front();
			return value;
		}
		std::optional<T> waitAndPop()
		{
			std::unique_lock<std::mutex> lock(mutex_);
			condVar_.wait(lock, [this] { return !deque_.empty(); });
			std::optional<T> v;
			v.emplace(std::move(deque_.front()));
			deque_.pop_front();

			return v;
		}



		bool empty() const
		{
			std::unique_lock<std::mutex> lock(mutex_);
			return deque_.empty();
		}

		void clear()
		{
			std::lock_guard<std::mutex> lock(mutex_);
			deque_.clear();
		}

		size_t size()
		{
			std::lock_guard<std::mutex> lock(mutex_);
			return deque_.size();
		}
		bool removeOne(const T& item)
		{
			std::lock_guard<std::mutex> lock(mutex_);
			deque_.erase(std::remove(deque_.begin(), deque_.end(), item), deque_.end());
			return true;
		}
	};

}