// 并发安全：多线程同时 submit，验证无数据竞争
#include "pool_test.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

TEST_F(PoolTest, ConcurrentSubmitSafety)
{
    std::vector<TaskResultPtr<int>> allResults;
    std::mutex mtx;
    std::vector<std::thread> threads;

    for (int t = 0; t < 8; ++t)
    {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 100; ++i)
            {
                auto r = pool.submit([t, i]() { return t * 1000 + i; });
                std::lock_guard<std::mutex> lk(mtx);
                allResults.push_back(r);
            }
        });
    }
    for (auto& t : threads) t.join();

    for (auto& r : allResults)
    {
        auto val = r->syncGetResult(5000);
        EXPECT_TRUE(val.has_value());
    }
    EXPECT_EQ(static_cast<int>(allResults.size()), 800);
}
