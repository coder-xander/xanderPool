#pragma once
#include <future>

#include "../xthread/xthread.h"

class XPool
{
private:
    std::mutex threadsMutex_;
    std::vector<XThreadPtr> threadsPool_;

public:
    explicit XPool(size_t threadCount = 10)
    {
        for (size_t i = 0; i < threadCount; i++)
        {
            threadsPool_.push_back(XThread::makeShared());
        }
    }
    ~XPool()
    {
        std::cout << "~XPool" << std::endl;
        threadsPool_.clear();
    }
    size_t nextThreadIndex_ = 0;
    ///@brief 线程池的调度，决定一个线程用于接受一个任务
    XThreadPtr decideAThread()
    {
        std::lock_guard lock(threadsMutex_);
        // get the thread at the current index
        XThreadPtr selectedThread = threadsPool_[nextThreadIndex_];
        // increment the index for next time, and wrap it around at the end
        // of the thread pool
        nextThreadIndex_ = (nextThreadIndex_ + 1) % threadsPool_.size();
        return selectedThread;
    }
    /// @brief 线程池接受一个任务
    template <typename F, typename... Args >
    TaskResultPtr addTask(F&& f, Args &&...args)
    {
        auto thread = decideAThread();
        // std::cout << "thread " << thread->getThreadId() << " accept this task" << std::endl;
        return  thread->addTask(std::forward<F>(f), std::forward<Args>(args)...);
    }

};