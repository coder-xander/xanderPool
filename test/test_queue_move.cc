// XDeque 值传递 move 语义回归测试：const T& → T 传值后应有真正的 move 语义
#include <gtest/gtest.h>
#include "queue.h"
#include <memory>

TEST(QueueTest, EnqueueMoveSemantics)
{
    // 用 unique_ptr 检测是否真正 move（不可 copy）
    xander::XDeque<std::unique_ptr<int>> q;
    auto p = std::make_unique<int>(42);
    q.enqueue(std::move(p));
    EXPECT_EQ(p, nullptr); // 原指针已被 move

    auto val = q.tryPop();
    ASSERT_TRUE(val.has_value());
    ASSERT_NE(*val, nullptr);
    EXPECT_EQ(**val, 42);
}
