#pragma once
#include "../xthread/xthread.h"
class XPool
{
private:
    std::vector<XThreadPtr> threadPool_;

public:
    XPool(size_t threadCount)
    {
        for (size_t i = 0; i < threadCount; ++i)
        {
            threadPool_.push_back(XThreadPtr(new XThread()));
        }
    }
    ~XPool()
    {
        // for (auto &thread : threadPool_)
        // {
        //     thread->exitFlag_.store(true);
        // }
    }
}