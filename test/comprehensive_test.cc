// 综合测试：覆盖所有核心功能和边界场景
#include "pool_test.cc"
#include "tool.h"
#include <atomic>
#include <chrono>
#include <set>
#include <thread>

using namespace std::chrono_literals;

// ====================================================================
// 基本提交：全局函数
// ====================================================================
TEST_F(PoolTest, SubmitGlobalFunction)
{
    auto asyncResult = pool.submit([](int n) -> long long {
        if (n <= 1) return n;
        long long a = 0, b = 1;
        for (int i = 2; i <= n; ++i) { auto t = a + b; a = b; b = t; }
        return b;
    }, 10);
    EXPECT_EQ(asyncResult->syncGetResult(), 55LL);
}

// ====================================================================
// 基本提交：成员函数
// ====================================================================
TEST_F(PoolTest, SubmitMemberFunction)
{
    auto asyncResult = pool.submit(&ClassA::memberFunction, &aobj, 1, 2);
    auto res = asyncResult->syncGetResult();
    EXPECT_EQ(res, aobj.memberFunction(1, 2));
}

// ====================================================================
// 基本提交：lambda
// ====================================================================
TEST_F(PoolTest, SubmitLambda)
{
    auto r = pool.submit([]() { return true; });
    EXPECT_TRUE(r->syncGetResult());
}

// ====================================================================
// 基本提交：仿函数
// ====================================================================
TEST_F(PoolTest, SubmitFunctor)
{
    auto r = pool.submit(aobj);
    auto res = r->syncGetResult();
    EXPECT_EQ(res, "ok");
}

// ====================================================================
// 优先级排序验证
// ====================================================================
TEST_F(PoolTest, PriorityOrderingVerification)
{
    Pool priorityPool(1, 1, 1000);

    std::mutex orderMutex;
    std::vector<int> executionOrder;
    std::atomic_bool blockerStarted{false};

    // 先提交一个阻塞任务占住唯一的 worker
    auto blocker = priorityPool.submit([&blockerStarted]()
    {
        blockerStarted.store(true);
        std::this_thread::sleep_for(200ms);
    });

    while (!blockerStarted.load())
        std::this_thread::sleep_for(5ms);

    // 排队提交：低、低、高、高
    auto low1 = priorityPool.submit(TaskBase::low, [&orderMutex, &executionOrder]()
    {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(1);
    });
    auto low2 = priorityPool.submit(TaskBase::low, [&orderMutex, &executionOrder]()
    {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(2);
    });
    auto high1 = priorityPool.submit(TaskBase::High, [&orderMutex, &executionOrder]()
    {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(3);
    });
    auto high2 = priorityPool.submit(TaskBase::High, [&orderMutex, &executionOrder]()
    {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(4);
    });

    blocker->syncGetResult(5000);
    high1->syncGetResult(5000);
    high2->syncGetResult(5000);
    low1->syncGetResult(5000);
    low2->syncGetResult(5000);

    ASSERT_EQ(executionOrder.size(), 4u);
    // 高优先级（3,4）应该在低优先级（1,2）之前
    int firstHigh = -1, firstLow = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (executionOrder[i] >= 3 && firstHigh == -1) firstHigh = i;
        if (executionOrder[i] <= 2 && firstLow == -1) firstLow = i;
    }
    EXPECT_LT(firstHigh, firstLow)
        << "Order: " << executionOrder[0] << "," << executionOrder[1]
        << "," << executionOrder[2] << "," << executionOrder[3];
}

// ====================================================================
// 多线程并发提交安全性
// ====================================================================
TEST_F(PoolTest, ConcurrentSubmitSafety)
{
    const int numThreads = 8;
    const int tasksPerThread = 100;
    std::vector<std::thread> threads;
    std::vector<TaskResultPtr<int>> allResults;
    std::mutex resultsMutex;

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&, t]()
        {
            for (int i = 0; i < tasksPerThread; ++i)
            {
                auto r = pool.submit([t, i]() { return t * 1000 + i; });
                std::lock_guard<std::mutex> lock(resultsMutex);
                allResults.push_back(r);
            }
        });
    }
    for (auto& t : threads) t.join();

    for (auto& r : allResults)
    {
        EXPECT_GE(r->syncGetResult(5000), 0);
    }
    EXPECT_EQ(static_cast<int>(allResults.size()), numThreads * tasksPerThread);
}

// ====================================================================
// submitSome 批量提交
// ====================================================================
TEST_F(PoolTest, SubmitSomeTest)
{
    // 用 TaskBasePtr 提交，通过 getTaskResult 获取结果
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
// 任务超时
// ====================================================================
TEST_F(PoolTest, TaskTimeoutTest)
{
    auto r = pool.submit([]() {
        std::this_thread::sleep_for(5s);
        return 42;
    });
    auto result = r->syncGetResult(100);
    EXPECT_FALSE(result.has_value());
}

// ====================================================================
// 动态扩缩容验证
// ====================================================================
TEST_F(PoolTest, DynamicWorkerScaling)
{
    Pool smallPool(1, 4, 100);

    std::vector<TaskResultPtr<void>> results;
    for (int i = 0; i < 20; ++i)
    {
        results.push_back(smallPool.submit([]() {
            std::this_thread::sleep_for(200ms);
        }));
    }

    auto dump = smallPool.dumpWorkers();
    EXPECT_FALSE(dump.empty());

    for (auto& r : results)
        r->syncGetResult(10000);
}

// ====================================================================
// 快速创建销毁 Pool
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
// findTasks 按名称查找
// ====================================================================
TEST_F(PoolTest, FindTasksByName)
{
    auto t1 = makeTask([]() { return 1; });
    t1->setName("findme");
    auto t2 = makeTask([]() { return 2; });
    t2->setName("findme");
    auto t3 = makeTask([]() { return 3; });
    t3->setName("other");

    pool.submit(t1);
    pool.submit(t2);
    pool.submit(t3);

    // 任务可能已被执行，只验证不崩溃
    EXPECT_NO_FATAL_FAILURE(pool.findTasks("findme"));

    t1->syncGetResult(5000);
    t2->syncGetResult(5000);
    t3->syncGetResult(5000);
}

// ====================================================================
// 1000 个空任务性能
// ====================================================================
TEST_F(PoolTest, PerformanceTest)
{
    std::vector<TaskResultPtr<TaskBase::Priority>> results;
    for (int i = 0; i < 1000; ++i)
    {
        results.push_back(pool.submit([]() {
            return randomPriority();
        }));
    }
    int completed = 0;
    for (auto& r : results)
    {
        auto res = r->syncGetResult(5000);
        if (res.has_value()) ++completed;
    }
    EXPECT_EQ(completed, 1000);
}
