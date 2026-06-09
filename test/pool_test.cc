#include <gtest/gtest.h>
#include <deque>
#include <future>
#include "pool.h"

using namespace std;
using namespace xander;

// 定义一个全局斐波那契函数

// 定义一个类A
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

// 定义测试套件
class PoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        pool.setWorkerExpiryTime(100); // 缩短 GC 周期，加速测试
    }

    //    void TearDown() override {
    //
    //    }

    Pool pool;
    ClassA aobj;
};
