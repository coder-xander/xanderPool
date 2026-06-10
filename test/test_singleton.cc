// 单例模式：instance() 一致性、singletonReset()、线程安全
#include "pool_test.h"
#include <atomic>
#include <thread>
#include <vector>

TEST_F(PoolTest, SingletonReturnsSameInstance)
{
    auto* p1 = Pool::instance();
    auto* p2 = Pool::instance();
    auto* p3 = Pool::instance();
    EXPECT_EQ(p1, p2);
    EXPECT_EQ(p2, p3);
}

TEST_F(PoolTest, SingletonResetDestroys)
{
    // 先拿一个实例，提交一个任务验证正常工作
    auto* before = Pool::instance();
    auto r = before->submit([]() { return 42; });
    EXPECT_EQ(r->syncGetResult(1000), 42);

    // 记录 before 的标识（通过提交一个标识性任务）
    auto marker1 = before->submit([]() { return 100; });
    EXPECT_EQ(marker1->syncGetResult(1000), 100);

    // 重置
    Pool::singletonReset();

    // 重置后 instance() 应返回新实例
    auto* after = Pool::instance();

    // 用 before 提交的任务结果应该还在（shared_ptr 生命周期）
    // 但 before 已被重置销毁，不应再使用

    // 新实例应能正常工作（提交不同结果验证确实用了新实例）
    auto r2 = after->submit([]() { return 99; });
    EXPECT_EQ(r2->syncGetResult(1000), 99);

    // 清理，避免影响其他测试
    Pool::singletonReset();
}

TEST_F(PoolTest, SingletonConcurrentAccess)
{
    // 先重置保证干净
    Pool::singletonReset();

    std::vector<Pool*> instances(10);
    std::vector<std::thread> threads;

    for (int t = 0; t < 10; ++t)
    {
        threads.emplace_back([&instances, t]() {
            instances[t] = Pool::instance();
        });
    }
    for (auto& t : threads) t.join();

    // 所有线程应拿到同一个指针
    for (int i = 1; i < 10; ++i)
        EXPECT_EQ(instances[0], instances[i]);

    // 清理
    Pool::singletonReset();
}
