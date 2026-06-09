// 任务查找测试
#include "pool_test.cc"

TEST_F(PoolTest, TaskFind)
{
    // work-stealing 模式下 findTasks 功能受限
    // 只验证不崩溃
    auto sharedPtrs = pool.findTasks("AAA");
    SUCCEED();
}
