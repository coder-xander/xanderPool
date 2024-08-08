//
// Created by oct-x on 2024/5/19.
//



#include "pool_test.cc"
#include "tool.h"
// 性能测试
TEST_F(PoolTest, PerformanceTest) {
    std::vector<TaskResultPtr<TaskBase::Priority>> asyncResult;
    for (int i = 0; i < 1000; ++i) {
        auto r = pool.submit([]() {
            std::cout << "hello, I am a worker" << std::endl;

            return randomPriority();
        });
        asyncResult.push_back(r);
    }

    std::vector<TaskBase::Priority> results;
    for (auto r: asyncResult) {
        auto res = r->syncGetResult();
        results.push_back(res);
    }
// 仅验证是否所有任务都成功完成
    EXPECT_EQ(results.size(), 1000);
}