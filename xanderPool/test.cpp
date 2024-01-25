#include <future>
#include "pool/xpool.h"
#include "worker/worker.h"
#include "tool.h"
#include "task/task.h"
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
		std::cout << "run member function !" << std::endl;
		return "function add :" + std::to_string(a) + std::to_string(b);
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
	std::vector<TaskResultPtr<void>> results;
	std::mutex resultsMutex_;
	//记录开始时间
	auto start = std::chrono::system_clock::now();
	for (int i = 0; i < 10; ++i)
	{
		auto f = std::async([&]()
			{

			});

	}
	// auto ds = xander::makeTask("ds",[]()
	// {
	// 		auto r = 2 + 34;
	// 	    return r;
	// });
	XPool * xPool = new XPool(2,12);
	std::deque<std::string> testDeq_;
	constexpr  int taskAddTestNum{ 120000 };//添加任务的数量
	timeTest("添加任务", [&]() mutable
		{
			for (int j = 0; j < taskAddTestNum; ++j)
			{
				auto r1 = xPool->submit([j]()
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					auto r = fib(20);
						// tstd::cout << "fib resul :" << std::to_string(r);
					},TaskBase::Priority::High);
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
	// system("pause");
	std::cout<<xPool->dumpWorkers() << std::endl;
	for (auto e : results)
	{
		std::lock_guard lock(resultsMutex_);
		e->syncGetValue();
	}
	system("pause");
	results.clear();
	// system("pause");
	return 0;

}
