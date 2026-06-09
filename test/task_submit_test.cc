// 提交测试
#include "pool_test.cc"

long long globalFibFunction(int n)
{
    if (n <= 1) return n;
    return globalFibFunction(n - 1) + globalFibFunction(n - 2);
}

TEST_F(PoolTest, SubmitGlobalFunction)
{
    auto asyncResult = pool.submit(globalFibFunction, 2);
    long long res = asyncResult->syncGetResult();
    EXPECT_EQ(res, globalFibFunction(2));
}

TEST_F(PoolTest, SubmitMemberFunction)
{
    auto asyncResult = pool.submit(&ClassA::memberFunction, &aobj, 1, 2);
    auto res = asyncResult->syncGetResult();
    EXPECT_EQ(res, aobj.memberFunction(1, 2));
}

TEST_F(PoolTest, SubmitLambdaFunction)
{
    auto lambdaFunction = []()
    {
        return true;
    };
    auto asyncResult = pool.submit(lambdaFunction);
    auto res = asyncResult->syncGetResult();
    EXPECT_EQ(res, true);
}

TEST_F(PoolTest, SubmitFunctor)
{
    auto asyncResult = pool.submit(aobj);
    auto res = asyncResult->syncGetResult();
    EXPECT_EQ(res, "ok");
}
