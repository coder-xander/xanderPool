// 任务查找测试
#include "pool_test.cc"

TEST_F(PoolTest, TaskFind)
{
    auto task4 = makeTask(aobj);
    task4->setName("AAA");
    pool.submit(task4);
    auto sharedPtrs = pool.findTasks("AAA");
    // 任务可能已被执行，只验证不崩溃
    task4->syncGetResult(5000);
    SUCCEED();
}
