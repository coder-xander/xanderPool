#pragma once
#include <future>

#include "../xthread/xthread.h"

class XPool
{
private:
    std::mutex threadsMutex_;
    std::vector<XThreadPtr> threadsPool_;

public:
    explicit XPool(size_t threadCount = 12)
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
    ///@brief 线程池的调度，决定一个线程用于接受一个任务
    XThreadPtr decideAThread()
    {
        std::lock_guard lock(threadsMutex_);
        for (XThreadPtr e : threadsPool_)
        {
            //如果空闲，直接用,也就是第一个闲着的线程
            if (e->isWaitting())
            {
                return e;
            }

        }
        //如果都不空闲，按照线程任务的数量，选择拥有任务数量最少的那个线程
        XThreadPtr res = threadsPool_.front();
        for (auto e : threadsPool_)
        {
            if (e->getTaskCount() <= res->getTaskCount())
            {
                res = e;
            }
        }
        return  res;
    }
    /// @brief 线程池接受一个任务
    template <typename F, typename... Args >
    std::future<std::invoke_result_t<F, Args...>> acceptTask(F&& function, Args &&...args)
    {
        auto thread = decideAThread();
       return  thread->acceptTask(std::forward<F>(function), std::forward<Args>(args ...));
    }

};