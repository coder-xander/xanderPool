#pragma once
#include <future>

#include "../xthread/xthread.h"

class XPool
{
private:
    std::mutex threadsMutex_;
    std::vector<XThreadPtr> threadsPool_;
    size_t nextThreadIndex_ = 0;
public:
    XPool()
    {
        //获取cpu核心数，创建这么多线程
        auto threadCount = std::thread::hardware_concurrency();
        for (size_t i = 0; i < threadCount; i++)
        {
            threadsPool_.push_back(XThread::makeShared());
        }


    }
    explicit XPool(size_t threadCount)
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
    ///@brief 增加一个工作的线程
    void addAWorkThread()
    {
        threadsPool_.push_back(XThread::makeShared());
    }
    ///@brief 减少一个工作的线程
    void removeAWorkThread()
    {
        //寻找一个空闲的线程，或者任务数量最少的线程
        auto thread = decideThreadIdlePriority();
        //移除这个线程
        thread->exit();
        threadsPool_.erase(std::find(threadsPool_.begin(), threadsPool_.end(), thread));
    }

    ///@brief 线程池的调度1，决定一个线程用于接受一个任务
    XThreadPtr decideAThreadAverage()
    {
        std::lock_guard lock(threadsMutex_);
        // get the thread at the current index
        XThreadPtr selectedThread = threadsPool_[nextThreadIndex_];
        // increment the index for next time, and wrap it around at the end
        // of the thread pool
        nextThreadIndex_ = (nextThreadIndex_ + 1) % threadsPool_.size();
        return selectedThread;
    }
    ///@brief 线程池的调度2，优先使用空闲线程,如果没有空闲线程，就选择任务最少的线程
    XThreadPtr decideThreadIdlePriority()
    {
        std::lock_guard lock(threadsMutex_);
        for (auto thread : threadsPool_)
        {
            if (thread->isWaiting())
            {
                return thread;
            }
        }
        XThreadPtr r = threadsPool_.front();
        for (auto thread : threadsPool_)
        {
            if (thread->getTaskCount() << r->getTaskCount())
            {
                r = thread;
            }
        }
        return r;
    }
    /// @brief 线程池接受一个任务
    template <typename F, typename... Args >
    TaskResultPtr addTask(F&& f, Args &&...args)
    {
        auto thread = decideThreadIdlePriority();
        // std::cout << "thread " << thread->getThreadId() << " accept this task" << std::endl;
        return  thread->addTask(std::forward<F>(f), std::forward<Args>(args)...);
    }

};