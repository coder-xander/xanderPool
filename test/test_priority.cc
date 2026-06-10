// 优先级排序测试：单 worker 下密度提交，验证高优先级任务先执行
#include "pool_test.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

using namespace std::chrono_literals;

TEST_F(PoolTest, PriorityOrdering)
{
    Pool p(1, 1, 1000); // 单 worker

    std::mutex orderMutex;
    std::vector<int> order;
    std::atomic_bool blockerStarted{false};

    auto blocker = p.submit([&blockerStarted]() {
        blockerStarted.store(true);
        std::this_thread::sleep_for(200ms);
    });
    while (!blockerStarted.load()) std::this_thread::sleep_for(5ms);

    p.submit(TaskBase::low, [&orderMutex, &order]() { std::lock_guard<std::mutex> lk(orderMutex); order.push_back(1); });
    p.submit(TaskBase::low, [&orderMutex, &order]() { std::lock_guard<std::mutex> lk(orderMutex); order.push_back(2); });
    p.submit(TaskBase::High, [&orderMutex, &order]() { std::lock_guard<std::mutex> lk(orderMutex); order.push_back(3); });
    p.submit(TaskBase::High, [&orderMutex, &order]() { std::lock_guard<std::mutex> lk(orderMutex); order.push_back(4); });

    blocker->syncGetResult(5000);
    std::this_thread::sleep_for(100ms); // 等任务执行完

    ASSERT_EQ(order.size(), 4u);
    // 高优先级在前
    int firstHigh = -1, firstLow = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (order[i] >= 3 && firstHigh == -1) firstHigh = i;
        if (order[i] <= 2 && firstLow == -1) firstLow = i;
    }
    EXPECT_LT(firstHigh, firstLow);
}
