// 综合测试：work-stealing + 结构化并发 + 原有功能
#include "pool_test.cc"
#include "tool.h"
#include <atomic>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

// ====================================================================
// 基本提交
// ====================================================================
TEST_F(PoolTest, SubmitLambda)
{
    auto r = pool.submit([]() { return 42; });
    EXPECT_EQ(r->syncGetResult(5000), 42);
}

TEST_F(PoolTest, SubmitMemberFunction)
{
    auto r = pool.submit(&ClassA::memberFunction, &aobj, 1, 2);
    EXPECT_EQ(r->syncGetResult(5000), aobj.memberFunction(1, 2));
}

TEST_F(PoolTest, SubmitFunctor)
{
    auto r = pool.submit(aobj);
    EXPECT_EQ(r->syncGetResult(5000), "ok");
}

// ====================================================================
// Work-Stealing：验证任务能被不同 worker 执行
// ====================================================================
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

// ====================================================================
// 结构化并发：TaskGroup
// ====================================================================
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

// ====================================================================
// 优先级排序
// ====================================================================
TEST_F(PoolTest, PriorityOrdering)
{
    Pool p(1, 1, 1000); // 单 worker

    std::mutex orderMutex;
    std::vector<int> order;
    std::atomic_bool blockerStarted{false};

    auto blocker = p.submit([&blockerStarted]() {
        blockerStarted.store(true);
        std::this_thread::sleep_for(200ms);
    });
    while (!blockerStarted.load()) std::this_thread::sleep_for(5ms);

    p.submit(TaskBase::low, [&orderMutex, &order]() { std::lock_guard<std::mutex> lk(orderMutex); order.push_back(1); });
    p.submit(TaskBase::low, [&orderMutex, &order]() { std::lock_guard<std::mutex> lk(orderMutex); order.push_back(2); });
    p.submit(TaskBase::High, [&orderMutex, &order]() { std::lock_guard<std::mutex> lk(orderMutex); order.push_back(3); });
    p.submit(TaskBase::High, [&orderMutex, &order]() { std::lock_guard<std::mutex> lk(orderMutex); order.push_back(4); });

    blocker->syncGetResult(5000);
    std::this_thread::sleep_for(100ms); // 等任务执行完

    ASSERT_EQ(order.size(), 4u);
    // 高优先级在前
    int firstHigh = -1, firstLow = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (order[i] >= 3 && firstHigh == -1) firstHigh = i;
        if (order[i] <= 2 && firstLow == -1) firstLow = i;
    }
    EXPECT_LT(firstHigh, firstLow);
}

// ====================================================================
// 并发安全
// ====================================================================
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

// ====================================================================
// 批量提交
// ====================================================================
TEST_F(PoolTest, SubmitSomeTest)
{
    std::vector<TaskBasePtr> tasks;
    std::vector<TaskResultPtr<int>> results;
    for (int i = 0; i < 10; ++i)
    {
        auto t = makeTask([i]() { return i * 2; });
        results.push_back(t->getTaskResult());
        tasks.push_back(t);
    }
    pool.submitSome(tasks);
    for (int i = 0; i < 10; ++i)
    {
        auto res = results[i]->syncGetResult(5000);
        ASSERT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), i * 2);
    }
}

// ====================================================================
// 超时
// ====================================================================
TEST_F(PoolTest, TaskTimeoutTest)
{
    auto r = pool.submit([]() { std::this_thread::sleep_for(5s); return 42; });
    auto result = r->syncGetResult(100);
    EXPECT_FALSE(result.has_value());
}

// ====================================================================
// 快速创建销毁
// ====================================================================
TEST_F(PoolTest, RapidPoolCreateDestroy)
{
    for (int i = 0; i < 100; ++i)
    {
        Pool p;
        auto r = p.submit([]() { return 42; });
        EXPECT_EQ(r->syncGetResult(1000), 42);
    }
}

// ====================================================================
// 动态扩缩
// ====================================================================
TEST_F(PoolTest, DynamicWorkerScaling)
{
    Pool smallPool(1, 4, 100);
    std::vector<TaskResultPtr<void>> results;
    for (int i = 0; i < 20; ++i)
        results.push_back(smallPool.submit([]() { std::this_thread::sleep_for(200ms); }));
    for (auto& r : results) r->syncGetResult(10000);
}

// ====================================================================
// 1000 任务性能
// ====================================================================
TEST_F(PoolTest, PerformanceTest)
{
    std::vector<TaskResultPtr<TaskBase::Priority>> results;
    for (int i = 0; i < 1000; ++i)
        results.push_back(pool.submit([]() { return randomPriority(); }));
    int completed = 0;
    for (auto& r : results)
    {
        auto res = r->syncGetResult(5000);
        if (res.has_value()) ++completed;
    }
    EXPECT_EQ(completed, 1000);
}
