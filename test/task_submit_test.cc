//
// Created by oct-x on 2024/5/19.
//

#include "pool_test.cc"

long long globalFibFunction(int n)
{
    if (n <= 1)
    {
        return n;
    }
    else
    {
        return globalFibFunction(n - 1) + globalFibFunction(n - 2);
    }
}

// 测试全局函数提交
TEST_F(PoolTest, SubmitGlobalFunction)
{
    auto asyncResult = pool.submit(globalFibFunction, 2);
    long long res = asyncResult->syncGetResult();
    EXPECT_EQ(res, globalFibFunction(2));
}

// 测试成员函数提交
TEST_F(PoolTest, SubmitMemberFunction)
{
    auto asyncResult = pool.submit(&ClassA::memberFunction, &aobj, 1, 2);
    auto res = static_cast<std::string>(asyncResult->syncGetResult());
    EXPECT_STREQ(res.data(), aobj.memberFunction(1, 2).data());
}

// 测试lambda函数提交
TEST_F(PoolTest, SubmitLambdaFunction)
{
    auto lambdaFunction = []()
    {
        std::cout << "lambda function" << std::endl;
        return true;
    };
    auto asyncResult = pool.submit(lambdaFunction);
    auto res = asyncResult->syncGetResult(); // 由于lambda没有返回值，只要保证没有异常抛出即可
    EXPECT_EQ(res, true);
}
//测试提交仿函数
TEST_F(PoolTest, SubmitFunctor)
{
    auto asyncResult = pool.submit(aobj);
    auto res = static_cast<std::string>(asyncResult->syncGetResult());
    SUCCEED();
}