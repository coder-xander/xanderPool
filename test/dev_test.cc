// 开发测试：快速创建销毁 Pool
#include "pool_test.h"

TEST_F(PoolTest, TestPool)
{
    for (int i = 0; i < 1000; ++i)
    {
        Pool p;
        auto af = p.submit([]() { return 42; });
        EXPECT_EQ(af->syncGetResult(1000), 42);
    }
}
