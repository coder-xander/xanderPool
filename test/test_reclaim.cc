// Worker 回收 + 死锁回归测试
#include "pool_test.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

TEST_F(PoolTest, WorkerReclamationNoDeadlock)
{
    // 小池子 + 快速回收，模拟 reclaim 触发时 worker 正在 steal
    for (int round = 0; round < 5; ++round)
    {
        Pool aggressivePool(1, 6, 50); // min=1, max=6, 50ms 回收
        std::atomic<int> done{0};

        // 提交一批长任务 + 大量短任务，迫使 stealing 频繁发生
        for (int i = 0; i < 10; ++i)
            aggressivePool.submit([&done]() {
                std::this_thread::sleep_for(std::chrono::microseconds(500 * (done.load() % 3 + 1)));
                done.fetch_add(1);
            })->syncGetResult(5000);

        EXPECT_EQ(done.load(), 10);
    }
}

TEST_F(PoolTest, ReclaimConcurrentWithSubmit)
{
    // 高并发提交 + 池子自动扩缩 → reclaim 与 stealing 重叠
    for (int round = 0; round < 3; ++round)
    {
        Pool p(2, 8, 30);
        std::atomic<int> counter{0};

        std::vector<std::thread> submitters;
        for (int t = 0; t < 4; ++t)
        {
            submitters.emplace_back([&p, &counter, t]() {
                for (int i = 0; i < 50; ++i)
                {
                    auto r = p.submit([&counter]() {
                        counter.fetch_add(1);
                        return 1;
                    });
                    r->syncGetResult(5000);
                }
            });
        }
        for (auto& t : submitters) t.join();
        EXPECT_EQ(counter.load(), 4 * 50);
    }
}
