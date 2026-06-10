// Task 扩展：copy 独立性、void 任务、优先级元数据
#include "pool_test.h"
#include <atomic>

// ================================================================
// Task::copy
// ================================================================

TEST_F(PoolTest, TaskCopyCreatesIndependentTask)
{
    // 创建任务并提交
    auto original = makeTask([]() { return 42; });
    pool.submit(original);

    // copy 并提交
    auto copy = original->copy();
    pool.submit(copy);

    EXPECT_EQ(original->getTaskResult()->syncGetResult(1000), 42);
    EXPECT_EQ(copy->getTaskResult()->syncGetResult(1000), 42);

    // copy 应有独立的 TaskResult
    EXPECT_NE(original->getTaskResult().get(), copy->getTaskResult().get());
}

TEST_F(PoolTest, TaskCopyWithPriorityBoost)
{
    auto task = makeTask(TaskBase::low, []() { return 1; });
    EXPECT_EQ(task->priority(), TaskBase::low);

    // copy + 提升优先级
    auto boosted = task->copy()->setPriority(TaskBase::High);
    EXPECT_EQ(boosted->priority(), TaskBase::High);
    // 原任务优先级不变
    EXPECT_EQ(task->priority(), TaskBase::low);

    // setPriority 返回 TaskBasePtr（丢失模板类型），通过原始 task 获取结果
    pool.submit(task);
    EXPECT_EQ(task->getTaskResult()->syncGetResult(1000), 1);
}

// ================================================================
// void 任务
// ================================================================

TEST_F(PoolTest, VoidTaskSubmit)
{
    std::atomic<int> sideEffect{0};
    auto r = pool.submit([&sideEffect]() {
        sideEffect.store(42);
    });

    // void 任务的 syncGetResult 无超时版本不返回 optional
    r->syncGetResult();
    EXPECT_EQ(sideEffect.load(), 42);
}

TEST_F(PoolTest, VoidTaskTimeout)
{
    std::atomic<int> sideEffect{0};
    auto r = pool.submit([&sideEffect]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        sideEffect.store(99);
    });

    // void 任务带超时的 syncGetResult 返回 void
    r->syncGetResult(5000);
    EXPECT_EQ(sideEffect.load(), 99);
}

// ================================================================
// 优先级元数据
// ================================================================

TEST_F(PoolTest, TaskPriorityDefaultsToNormal)
{
    auto task = makeTask([]() { return 0; });
    EXPECT_EQ(task->priority(), TaskBase::Normal);
}

TEST_F(PoolTest, TaskSetPriorityReturnsSelf)
{
    auto task = makeTask([]() { return 0; });
    // setPriority 返回 shared_from_this，支持链式调用
    auto* ptr = task->setPriority(TaskBase::High).get();
    EXPECT_EQ(ptr, task.get());
    EXPECT_EQ(task->priority(), TaskBase::High);
}

// ================================================================
// 预创建任务 + submit
// ================================================================

TEST_F(PoolTest, SubmitPreMadeTask)
{
    auto task = makeTask(TaskBase::High, []() { return 7; });
    pool.submit(task);
    EXPECT_EQ(task->getTaskResult()->syncGetResult(1000), 7);
}

TEST_F(PoolTest, SubmitPreMadeTaskWithoutPriorityOverride)
{
    auto task = makeTask([]() { return 3; });
    pool.submit(task);
    EXPECT_EQ(task->getTaskResult()->syncGetResult(1000), 3);
}

// ================================================================
// syncGetResult 阻塞等待（无超时）
// ================================================================

TEST_F(PoolTest, SyncGetResultBlocks)
{
    auto r = pool.submit([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return 123;
    });
    int val = r->syncGetResult();
    EXPECT_EQ(val, 123);
}

TEST_F(PoolTest, TaskNameMetadata)
{
    auto task = makeTask([]() { return 0; });
    EXPECT_TRUE(task->name().empty());

    task->setName("my-task");
    EXPECT_EQ(task->name(), "my-task");
}
