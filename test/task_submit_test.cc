// 提交测试
#include "pool_test.h"

long long globalFibFunction(int n)
{
    if (n <= 1) return n;
    return globalFibFunction(n - 1) + globalFibFunction(n - 2);
}

TEST_F(PoolTest, SubmitGlobalFunction)
{
    auto r = pool.submit(globalFibFunction, 2);
    EXPECT_EQ(r->syncGetResult(5000), globalFibFunction(2));
}

TEST_F(PoolTest, SubmitMemberFunction)
{
    auto r = pool.submit(&ClassA::memberFunction, &aobj, 1, 2);
    EXPECT_EQ(r->syncGetResult(5000), aobj.memberFunction(1, 2));
}

TEST_F(PoolTest, SubmitLambdaFunction)
{
    auto r = pool.submit([]() { return true; });
    EXPECT_EQ(r->syncGetResult(5000), true);
}

TEST_F(PoolTest, SubmitFunctor)
{
    auto r = pool.submit(aobj);
    EXPECT_EQ(r->syncGetResult(5000), "ok");
}
