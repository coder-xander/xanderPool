// Pool 配置与查询方法：静态模式、min==max、查询 API
#include "pool_test.h"
#include <thread>
#include <chrono>

TEST_F(PoolTest, UseStaticModeFixedCount)
{
    Pool p(1, 8, 100);
    p.useStaticMode(3); // 固定 3 个 worker

    // 提交任务，验证都能完成
    for (int i = 0; i < 30; ++i)
    {
        auto r = p.submit([i]() { return i; });
        EXPECT_EQ(r->syncGetResult(1000), i);
    }
}

TEST_F(PoolTest, MinEqualsMaxConfig)
{
    // min == max 时不应创建或回收 worker
    Pool p(4, 4, 50);

    auto r = p.submit([]() { return 100; });
    EXPECT_EQ(r->syncGetResult(1000), 100);
}

TEST_F(PoolTest, PoolQueryTotalTaskCount)
{
    EXPECT_EQ(pool.totalTaskCount(), 0u);

    auto r1 = pool.submit([]() { return 1; });
    auto r2 = pool.submit([]() { return 2; });
    r1->syncGetResult(1000);
    r2->syncGetResult(1000);

    // totalTaskCount 是累计提交数，不是未完成数
    EXPECT_GE(pool.totalTaskCount(), 2u);
}

TEST_F(PoolTest, PoolGetAnIdleWorker)
{
    // 提交一个短任务，完成后 worker 变为 idle
    pool.submit([]() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); })
        ->syncGetResult(1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto w = pool.getAnIdleWorker();
    ASSERT_NE(w, nullptr);
    EXPECT_TRUE(w->isIdle());
}

TEST_F(PoolTest, PoolCustomExpiryTime)
{
    Pool p(2, 6, 50);
    p.setWorkerExpiryTime(200);

    auto r = p.submit([]() { return 42; });
    EXPECT_EQ(r->syncGetResult(1000), 42);
}
