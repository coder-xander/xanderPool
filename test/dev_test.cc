//
// Created by oct-x on 2024/5/20.
//
#include "pool_test.cc"

TEST_F(PoolTest, TestPool)
{
    for (int i = 0; i < 1000; ++i)
    {
        Pool pool;
        auto af = pool.submit([]() { std::cout << "dev" << std::endl; });
        af->syncGetResult();
    }
}
