// WorkStealingDeque 全面测试：所有公开方法、LIFO/FIFO 顺序、并发安全
#include <gtest/gtest.h>
#include "work_stealing.h"
#include <atomic>
#include <thread>

// ================================================================
// WorkStealingDeque
// ================================================================

TEST(WorkStealingDequeTest, BasicOps)
{
    xander::WorkStealingDeque<int> dq;
    EXPECT_TRUE(dq.empty());
    EXPECT_EQ(dq.size(), 0u);

    dq.push(1);
    dq.push(2);
    dq.push(3);
    EXPECT_FALSE(dq.empty());
    EXPECT_EQ(dq.size(), 3u);

    // pop 是 LIFO（owner 端）
    auto v1 = dq.pop();
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(*v1, 3);

    auto v2 = dq.pop();
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(*v2, 2);

    auto v3 = dq.pop();
    ASSERT_TRUE(v3.has_value());
    EXPECT_EQ(*v3, 1);

    EXPECT_TRUE(dq.empty());

    dq.push(10);
    dq.clear();
    EXPECT_TRUE(dq.empty());
}

TEST(WorkStealingDequeTest, StealOrder)
{
    // steal 应从 back（最早入队的）取 — FIFO 语义
    xander::WorkStealingDeque<int> dq;
    dq.push(1);  // front → [1]
    dq.push(2);  // front → [2, 1]
    dq.push(3);  // front → [3, 2, 1]
    dq.push(4);  // front → [4, 3, 2, 1]
    dq.push(5);  // front → [5, 4, 3, 2, 1]

    // steal 从 back: 1, 2, 3...
    auto s1 = dq.steal();
    ASSERT_TRUE(s1.has_value());
    EXPECT_EQ(*s1, 1); // 最早入队的

    auto s2 = dq.steal();
    ASSERT_TRUE(s2.has_value());
    EXPECT_EQ(*s2, 2);

    // pop 从 front: 5, 4...
    auto p1 = dq.pop();
    ASSERT_TRUE(p1.has_value());
    EXPECT_EQ(*p1, 5);

    auto p2 = dq.pop();
    ASSERT_TRUE(p2.has_value());
    EXPECT_EQ(*p2, 4);
}

TEST(WorkStealingDequeTest, StealOnEmpty)
{
    xander::WorkStealingDeque<int> dq;
    auto s = dq.steal();
    EXPECT_FALSE(s.has_value());
}

TEST(WorkStealingDequeTest, PopOnEmpty)
{
    xander::WorkStealingDeque<int> dq;
    auto p = dq.pop();
    EXPECT_FALSE(p.has_value());
}

TEST(WorkStealingDequeTest, SizeConst)
{
    const xander::WorkStealingDeque<int> dq;
    EXPECT_TRUE(dq.empty());
    EXPECT_EQ(dq.size(), 0u);
}

TEST(WorkStealingDequeTest, ConcurrentPushPopSteal)
{
    xander::WorkStealingDeque<int> dq;

    // 先 push 全部，再并发 pop/steal，避免生产者-消费者竞态
    for (int i = 0; i < 1000; ++i)
        dq.push(i);

    std::atomic<int64_t> popSum{0};
    std::atomic<int64_t> stealSum{0};
    std::atomic<int>     popCount{0};
    std::atomic<int>     stealCount{0};

    std::thread popper([&dq, &popSum, &popCount]() {
        while (true)
        {
            auto v = dq.pop();
            if (!v.has_value()) break;
            popSum.fetch_add(*v);
            popCount.fetch_add(1);
        }
    });

    std::thread stealer([&dq, &stealSum, &stealCount]() {
        while (true)
        {
            auto v = dq.steal();
            if (!v.has_value()) break;
            stealSum.fetch_add(*v);
            stealCount.fetch_add(1);
        }
    });

    popper.join();
    stealer.join();

    EXPECT_EQ(popCount.load() + stealCount.load(), 1000);
    EXPECT_EQ(popSum.load() + stealSum.load(), 499500);
}
