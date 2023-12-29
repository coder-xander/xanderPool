#pragma once
#include "../xthread/xthread.h"
class XPool
{
private:
    std::vector<XThreadPtr> threadPool_;

public:
    explicit XPool(size_t threadCount = 12)
    {
        for (size_t i = 0; i < threadCount; i++)
        {
            threadPool_.push_back(XThread::makeShared());
        }
    }
    ~XPool()
    {
        std::cout << "~XPool" << std::endl;
        threadPool_.clear();
    }

    XThreadPtr avaiableThread()
    {
        // 调度
        }
    template <typename F, typename... Args>
    TaskId acceptTask(F &&function, Args &&...args)
    {
    }
}