#pragma once
#include <mutex>
#include <atomic>
#include <condition_variable>
class XLock
{
public:
    XLock(int count) : count_(count) {}

    void acquire();

    void release();

private:
    std::mutex mutex_;
    std::atomic_int count_;
    std::condition_variable con_;
};
