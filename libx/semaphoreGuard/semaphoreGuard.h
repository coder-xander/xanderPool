#pragma once
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace xander
{

    class SemaphoreGuard
    {
    public:
        SemaphoreGuard(int count) : count_(count) {}
        void acquire()
        {
            std::unique_lock<std::mutex> lock(mutex_);
            while (count_ <= 0)
            {
                con_.wait(lock);
            }
            --count_;
        }
        void release()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++count_;
            con_.notify_one();
        }
        ~SemaphoreGuard()
        {
            // std::cout<<"~XSemaphoreGuard"<<std::endl;
        }
        void clear()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            count_ = 0;
        }
    private:
        std::mutex mutex_;
        int count_;
        std::condition_variable con_;
    };

}