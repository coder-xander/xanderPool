#include <mutex>
#include "xlock.h"
void XLock::acquire()
{
        std::unique_lock<std::mutex> lock(mutex_);
        while (count_ <= 0)
        {
                con_.wait(lock);
        }
        --count_;
}
void XLock::release()
{
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
        con_.notify_one();
}