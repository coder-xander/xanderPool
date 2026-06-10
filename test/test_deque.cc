// XDeque + WorkStealingDeque 全面测试：所有公开方法、并发访问、steal 顺序
#include <gtest/gtest.h>
#include "queue.h"
#include "work_stealing.h"
#include <memory>
#include <thread>
#include <vector>
#include <atomic>

// ================================================================
// XDeque
// ================================================================

TEST(XDequeTest, BasicOps)
{
    xander::XDeque<int> q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);

    q.enqueue(1);
    q.enqueue(2);
    q.enqueue(3);
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 3u);

    auto v1 = q.tryPop();
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(*v1, 1); // FIFO: first in, first out

    auto v2 = q.tryPop();
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(*v2, 2);

    q.clear();
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
}

TEST(XDequeTest, CopyConstructor)
{
    xander::XDeque<int> q1;
    q1.enqueue(10);
    q1.enqueue(20);

    xander::XDeque<int> q2(q1); // 拷贝构造

    auto v1 = q2.tryPop();
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(*v1, 10);

    auto v2 = q2.tryPop();
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(*v2, 20);

    EXPECT_TRUE(q2.empty());
    // 原队列不受影响
    EXPECT_FALSE(q1.empty());
}

TEST(XDequeTest, RemoveOne)
{
    xander::XDeque<int> q;
    q.enqueue(1);
    q.enqueue(2);
    q.enqueue(3);
    q.enqueue(2);
    q.enqueue(4);

    q.removeOne(2); // 移除所有 2
    EXPECT_EQ(q.size(), 3u);

    auto v1 = q.tryPop();
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(*v1, 1);

    auto v2 = q.tryPop();
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(*v2, 3);

    auto v3 = q.tryPop();
    ASSERT_TRUE(v3.has_value());
    EXPECT_EQ(*v3, 4);
}

TEST(XDequeTest, TryPopOnEmpty)
{
    xander::XDeque<int> q;
    auto v = q.tryPop();
    EXPECT_FALSE(v.has_value());
}

TEST(XDequeTest, EnqueueMoveSemantics)
{
    // 用 unique_ptr 确认真正 move（不可 copy）
    xander::XDeque<std::unique_ptr<int>> q;
    auto p = std::make_unique<int>(42);
    q.enqueue(std::move(p));
    EXPECT_EQ(p, nullptr); // 原指针已被 move

    auto val = q.tryPop();
    ASSERT_TRUE(val.has_value());
    ASSERT_NE(*val, nullptr);
    EXPECT_EQ(**val, 42);
}

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

// ================================================================
// 并发
// ================================================================

TEST(XDequeTest, ConcurrentPushPop)
{
    xander::XDeque<int> q;
    std::atomic<int> sum{0};
    std::vector<std::thread> producers;

    // 4 个 producer
    for (int t = 0; t < 4; ++t)
    {
        producers.emplace_back([&q, t]() {
            for (int i = 0; i < 500; ++i)
                q.enqueue(t * 1000 + i);
        });
    }
    for (auto& t : producers) t.join();

    EXPECT_EQ(q.size(), 2000u);

    // 4 个 consumer（用独立 vector，避免重复 join 已结束的线程）
    std::vector<std::thread> consumers;
    std::atomic<int> consumed{0};
    for (int t = 0; t < 4; ++t)
    {
        consumers.emplace_back([&q, &consumed]() {
            while (true)
            {
                auto v = q.tryPop();
                if (!v.has_value()) break;
                consumed.fetch_add(1);
            }
        });
    }
    for (auto& t : consumers) t.join();

    EXPECT_EQ(consumed.load(), 2000);
}

TEST(WorkStealingDequeTest, ConcurrentPushPopSteal)
{
    xander::WorkStealingDeque<int> dq;

    // 先 push 全部 1000 个元素，再启动 pop/steal，避免生产者-消费者竞态
    for (int i = 0; i < 1000; ++i)
        dq.push(i);

    std::atomic<int64_t> popSum{0};
    std::atomic<int64_t> stealSum{0};
    std::atomic<int>     popCount{0};
    std::atomic<int>     stealCount{0};

    // pop/steal 各做一部分，直到 deque 被 drain 完毕
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

    // pop + steal 的总数应为 1000
    int totalTaken = popCount.load() + stealCount.load();
    EXPECT_EQ(totalTaken, 1000);
    // pop + steal 的和应为 0+1+2+...+999 = 499500
    EXPECT_EQ(popSum.load() + stealSum.load(), 499500);
}
