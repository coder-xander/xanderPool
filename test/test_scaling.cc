// 动态扩缩容：小池子 + 多任务，验证 worker 自动扩展
#include "pool_test.h"
#include <chrono>
#include <vector>

using namespace std::chrono_literals;

TEST_F(PoolTest, DynamicWorkerScaling)
{
    Pool smallPool(1, 4, 100);
    std::vector<TaskResultPtr<void>> results;
    for (int i = 0; i < 20; ++i)
        results.push_back(smallPool.submit([]() { std::this_thread::sleep_for(200ms); }));
    for (auto& r : results) r->syncGetResult(10000);
}
