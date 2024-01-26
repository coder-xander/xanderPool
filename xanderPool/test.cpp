#include <deque>
#include <future>
#include "xanderPool/include/xpool.h"
#include "xanderPool/include/worker.h"
#include "tool.h"
#include "xanderPool/include/task.h"
using namespace std;
using namespace xander;
class ClassA
{
public:
	ClassA()
	{
		std::cout << "CA()" << std::endl;
	}
	~ClassA()
	{
		std::cout << "~CA()" << std::endl;
	}
	std::string memberFunc(int a, double b)
	{
		this_thread::sleep_for(std::chrono::milliseconds(300));
		return  std::to_string(a) + std::to_string(b);
	}
};
// 计算斐波那契数列的函数
long long fib(int n)
{
	if (n <= 1)
		return n;
	else
		return fib(n - 1) + fib(n - 2);
}

int main()
{

	ClassA ca;
	// 添加一个全局函数
	std::vector<TaskResultPtr<long long>> results;
	std::mutex resultsMutex_;


	// auto ds = xander::makeTask("ds",[]()
	// {
	// 		auto r = 2 + 34;
	// 	    return r;
	// });
	XPool * xPool = new XPool(2,12);
	deque<std::string> testDeq_;

	constexpr  int taskAddTestNum{ 100 };//添加任务的数量
	auto f =  std::async(launch::async, [xPool, &results, &resultsMutex_]()
	{
			for (int j = 0; j < taskAddTestNum; ++j)
			{
				auto r1 = xPool->submit([j]()
					{
						// std::this_thread::sleep_for(std::chrono::milliseconds(20));
						auto r = fib(12);
						std::cout << "sub thread added \n";
						return r;
					}, TaskBase::Priority::Normal);
				std::lock_guard lock(resultsMutex_);
				results.push_back(r1);
			}

	});
	timeTest("添加任务", [&]() mutable
		{
			for (int j = 0; j < taskAddTestNum; ++j)
			{
				auto r1 = xPool->submit([j]()
					{
						// std::this_thread::sleep_for(std::chrono::milliseconds(20));
						auto r = fib(12);
						std::cout << "main thread added \n";

						return r;
					}, TaskBase::Priority::Normal);
				std::lock_guard lock(resultsMutex_);

				results.push_back(r1);
			}
			
		
		});
	

	// timeTest("生成uuid100000个", [&]()
	// {
	// 		std::vector<std::string> uuids;
	// 	for (int i = 0; i < 100000; ++i)
	// 	{
	// 		uuids.push_back(Worker::generateUUID());
	// 	}
	// });
	// for (int i = 0; i < testTimes_; ++i)
	// {
	// 	testDeq_.push_back("dsadsadsadasdsa");
	//
	// }
	system("pause");
	std::cout<<xPool->dumpWorkers() << std::endl;
	for (auto e : results)
	{
		std::lock_guard lock(resultsMutex_);
		e->syncGetResult();
	}
	system("pause");
	delete xPool;
	results.clear();
	system("pause");
	return 0;

}
