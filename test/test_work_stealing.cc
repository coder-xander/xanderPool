// Work-Stealing：验证任务能被不同 worker 执行，负载均衡
#include "pool_test.h"
#include <atomic>
#include <chrono>
#include <future>

using namespace std::chrono_literals;

TEST_F(PoolTest, WorkStealingBasic)
{
    // 提交大量短任务，验证全部完成
    std::vector<TaskResultPtr<int>> results;
    for (int i = 0; i < 200; ++i)
    {
        results.push_back(pool.submit([i]() { return i; }));
    }
    for (int i = 0; i < 200; ++i)
    {
        auto val = results[i]->syncGetResult(5000);
        ASSERT_TRUE(val.has_value());
        EXPECT_EQ(val.value(), i);
    }
}

TEST_F(PoolTest, WorkStealingUnevenLoad)
{
    // 提交几个长任务 + 大量短任务
    // work-stealing 应该能平衡负载
    std::vector<TaskResultPtr<void>> results;

    // 2 个长任务
    for (int i = 0; i < 2; ++i)
    {
        results.push_back(pool.submit([]() {
            std::this_thread::sleep_for(200ms);
        }));
    }

    // 50 个短任务
    for (int i = 0; i < 50; ++i)
    {
        results.push_back(pool.submit([]() {
            std::this_thread::sleep_for(1ms);
        }));
    }

    for (auto& r : results)
        r->syncGetResult(10000);
}

TEST_F(PoolTest, StressStealAndReclaim)
{
    // 大量任务 + 短回收时间，全面触发 work-stealing 和 reclaim
    Pool stressPool(1, 12, 20);
    std::atomic<int64_t> sum{0};

    std::vector<std::future<void>> futures;
    for (int t = 0; t < 6; ++t)
    {
        futures.push_back(std::async(std::launch::async, [&stressPool, &sum, t]() {
            for (int i = 0; i < 100; ++i)
            {
                auto r = stressPool.submit([&sum, val = i + t * 1000]() {
                    sum.fetch_add(val);
                    // 模拟不同时长的工作，增加 steal 机会
                    std::this_thread::sleep_for(std::chrono::microseconds(val % 50));
                    return val;
                });
                r->syncGetResult(5000);
            }
        }));
    }
    for (auto& f : futures) f.wait();

    // 验证总和：每个任务 val 累加一次
    int64_t expected = 0;
    for (int t = 0; t < 6; ++t)
        for (int i = 0; i < 100; ++i)
            expected += i + t * 1000;
    EXPECT_EQ(sum.load(), expected);
}
