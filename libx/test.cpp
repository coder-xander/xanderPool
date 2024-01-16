

#include <future>

#include "xpool/xpool.h"
#include "xtask/taskManager/taskManager.h"


using namespace std;
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

	XPool xpoPool;

	ClassA ca;
	// 添加一个全局函数
	std::vector<TaskResultPtr> results;

	//记录开始时间
	auto start = std::chrono::system_clock::now();
	for (int j = 0; j < 10000; ++j)
	{
		auto r1 = xpoPool.addTask([j](int x, int y)
			{
			// std::cout << "run lambda !"<<"线程id"<<std::this_thread::get_id()<< std::endl;

				std::thread::id this_id = std::this_thread::get_id();
				std::ostringstream ss;
				ss << this_id;
				std::string strThreadId = ss.str();
		return "lamda add res num :" + std::to_string(j)+"来自线程: "+ strThreadId; },
			1, 2);
		// 添加一个成员函数
		// auto r2 = xpoPool.acceptTask(&ClassA::memberFunc, &ca, 1, 3.4);
		results.push_back(r1);
		// results.push_back(r2);
	}
	//记录时间差，打印添加任务花费的时间
	auto end = std::chrono::system_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	std::cout << "添加任务花费的时间：" << duration << "ms" << std::endl;

	// 也可以添加全局函数
	for (auto e : results)
	{

		std::cout << "获得结果：" << e->toString() << std::endl;

	}
	std::cout << "处理完成 一共任务:" << std::to_string(results.size()) << std::endl;
	system("pause");
	return 0;
}
