// 性能测试
#include "pool_test.h"
#include "tool.h"

TEST_F(PoolTest, PerformanceSubmit1000)
{
    std::vector<TaskResultPtr<TaskBase::Priority>> asyncResult;
    for (int i = 0; i < 1000; ++i)
        asyncResult.push_back(pool.submit([]() { return randomPriority(); }));
    int completed = 0;
    for (auto& r : asyncResult)
    {
        auto res = r->syncGetResult(5000);
        if (res.has_value()) ++completed;
    }
    EXPECT_EQ(completed, 1000);
}
