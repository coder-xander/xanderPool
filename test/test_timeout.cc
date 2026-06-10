// 超时测试：syncGetResult(ms) 超时返回 false
#include "pool_test.h"
#include <chrono>

using namespace std::chrono_literals;

TEST_F(PoolTest, TaskTimeoutTest)
{
    auto r = pool.submit([]() { std::this_thread::sleep_for(5s); return 42; });
    auto result = r->syncGetResult(100);
    EXPECT_FALSE(result.has_value());
}
