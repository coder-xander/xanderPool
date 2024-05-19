//
// Created by oct-x on 2024/5/19.
//


#include "pool_test.cc"
long long globalFibFunction(int n) {
    if (n <= 1) {
        return n;
    } else {
        return globalFibFunction(n - 1) + globalFibFunction(n - 2);
    }
}

// 测试任务的提交
TEST_F(PoolTest, TaskFind) {
    auto task4 = makeTask(aobj);
    task4->setName("AAA");
    pool.submit(task4);
    auto sharedPtrs = pool.findTasks("AAA");
    int test_value  =static_cast<int>(sharedPtrs.size());
    EXPECT_EQ(test_value, 1);
}
