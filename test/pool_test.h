#pragma once
#include <gtest/gtest.h>
#include "pool.h"

using namespace std;
using namespace xander;

class ClassA
{
public:
    std::string memberFunction(int a, double b)
    {
        this_thread::sleep_for(std::chrono::milliseconds(1));
        return std::to_string(a) + std::to_string(b);
    }

    std::string operator()()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return "ok";
    }
};

class PoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        pool.setWorkerExpiryTime(100); // 快速回收用于测试
    }

    Pool pool;
    ClassA aobj;
};
