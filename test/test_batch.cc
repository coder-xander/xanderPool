// 批量提交测试：submitSome
#include "pool_test.h"
#include <vector>

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
