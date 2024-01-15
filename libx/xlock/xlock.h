#pragma once
#include <mutex>
#include <atomic>
#include <condition_variable>
class XLock
{
public:
    XLock(int count) : count_(count) {}
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
private:
    std::mutex mutex_;
    int count_;
    std::condition_variable con_;
};
