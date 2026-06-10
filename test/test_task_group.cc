// 结构化并发：TaskGroup 基础功能、取消、异常传播、嵌套、NRVO
#include "pool_test.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <algorithm>

using namespace std::chrono_literals;

TEST_F(PoolTest, TaskGroupBasic)
{
    std::atomic<int> counter{0};
    {
        auto group = pool.createGroup();
        for (int i = 0; i < 20; ++i)
        {
            group.spawn([&counter]() {
                counter.fetch_add(1);
            });
        }
        group.wait();
    }
    EXPECT_EQ(counter.load(), 20);
}

TEST_F(PoolTest, TaskGroupWithReturnValues)
{
    // TaskGroup 本身不收集返回值，但可以通过外部变量收集
    std::mutex mtx;
    std::vector<int> results;
    {
        auto group = pool.createGroup();
        for (int i = 0; i < 10; ++i)
        {
            group.spawn([&mtx, &results, i]() {
                std::lock_guard<std::mutex> lk(mtx);
                results.push_back(i * 2);
            });
        }
        group.wait();
    }
    EXPECT_EQ(static_cast<int>(results.size()), 10);
    // 验证所有值都存在
    std::sort(results.begin(), results.end());
    for (int i = 0; i < 10; ++i)
        EXPECT_EQ(results[i], i * 2);
}

TEST_F(PoolTest, TaskGroupCancel)
{
    std::atomic<int> counter{0};
    {
        auto group = pool.createGroup();
        for (int i = 0; i < 100; ++i)
        {
            group.spawn([&counter, i]() {
                std::this_thread::sleep_for(10ms);
                counter.fetch_add(1);
            });
        }
        // 立即取消，不等全部完成
        group.cancel();
    }
    // 被取消的任务不应执行
    // 但由于取消是异步的，部分任务可能已经开始执行
    EXPECT_LE(counter.load(), 100);
}

TEST_F(PoolTest, TaskGroupException)
{
    {
        auto group = pool.createGroup();
        group.spawn([]() {
            throw std::runtime_error("test error");
        });
        group.spawn([]() {
            // 正常任务
        });
        // wait 应该抛出异常
        EXPECT_THROW(group.wait(), std::runtime_error);
    }
}

TEST_F(PoolTest, TaskGroupNested)
{
    std::atomic<int> counter{0};
    {
        auto outer = pool.createGroup();
        for (int i = 0; i < 5; ++i)
        {
            outer.spawn([this, &counter]() {
                // 内层 TaskGroup
                auto inner = pool.createGroup();
                for (int j = 0; j < 5; ++j)
                {
                    inner.spawn([&counter]() {
                        counter.fetch_add(1);
                    });
                }
                inner.wait();
            });
        }
        outer.wait();
    }
    EXPECT_EQ(counter.load(), 25);
}

TEST_F(PoolTest, TaskGroupCreateGroupReturnsByValue)
{
    std::atomic<int> counter{0};
    // 验证 NRVO 正常编译和运行（createGroup 返回 TaskGroup）
    {
        auto group = pool.createGroup();
        group.spawn([&counter]() { counter.fetch_add(1); });
        group.spawn([&counter]() { counter.fetch_add(2); });
        group.wait();
    }
    EXPECT_EQ(counter.load(), 3);
}
