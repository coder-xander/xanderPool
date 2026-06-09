// 性能测试
#include "pool_test.cc"
#include "tool.h"

TEST_F(PoolTest, PerformanceSubmit1000)
{
    std::vector<TaskResultPtr<TaskBase::Priority>> asyncResult;
    for (int i = 0; i < 1000; ++i)
    {
        auto r = pool.submit([]() {
            return randomPriority();
        });
        asyncResult.push_back(r);
    }

    int completed = 0;
    for (auto& r : asyncResult)
    {
        auto res = r->syncGetResult(5000);
        if (res.has_value()) ++completed;
    }
    EXPECT_EQ(completed, 1000);
}
