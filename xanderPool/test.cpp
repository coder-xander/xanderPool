#include <future>
#include "pool/xpool.h"
#include "worker/worker.h"
#include "tool.h"
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

	std::deque<std::string> testDeq_;
	constexpr  int taskAddTestNum{ 120000 };//添加任务的数量
	// timeTest("添加任务", [&]() mutable
	// 	{
	// 		for (int j = 0; j < taskAddTestNum; ++j)
	// 		{
	// 			auto r1 = XPool::instance()->submit([j]()
	// 				{
	// 					// std::cout << "run lambda !"<<"线程id"<<std::this_thread::get_id()<< std::endl;
	// 					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	// 					std::thread::id this_id = std::this_thread::get_id();
	// 					std::ostringstream ss;
	// 					ss << this_id;
	// 					std::string strThreadId = ss.str();
	// 					// std::cout << "running" << std::to_string(j) + "来自线程: " + strThreadId << std::endl;
	// 					// return "lamda add res num :" + std::to_string(j) + "来自线程: " + strThreadId;
	// 				});
	//
	// 			results.push_back(r1);
	// 		}
	//
	// 	});
	timeTest("生成uuid100000个", [&]()
	{
			std::vector<std::string> uuids;
		for (int i = 0; i < 100000; ++i)
		{
			uuids.push_back(Worker::generateUUID());
		}
	});
	// for (int i = 0; i < testTimes_; ++i)
	// {
	// 	testDeq_.push_back("dsadsadsadasdsa");
	//
	// }
	// system("pause");
	std::cout << XPool::instance()->dumpWorkers() << std::endl;
	for (auto e : results)
	{
		std::lock_guard lock(resultsMutex_);
		 e->syncGetValue();
		// std::cout << "获得结果：" << s << std::endl;

	}
	results.clear();
	system("pause");
	// system("pause");
	return 0;

}
