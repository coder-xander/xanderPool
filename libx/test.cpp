
#include "test.h"
#include "xtask/taskManager/taskManager.h"
#include <string>

using namespace std;
class CA
{
public:
	CA()
	{
		std::cout << "CA()" << std::endl;
	}
	~CA()
	{
		std::cout << "~CA()" << std::endl;
	}
	std::string memberFunc(int a, double b)
	{
		return std::to_string(a) + std::to_string(b);
	}
};
int main()
{
	// 记录开始时间点
	auto start = std::chrono::high_resolution_clock::now();

	TaskManager tm;
	CA ca;

	for (size_t i = 0; i < 100; i++)
	{
		// 添加一个全局函数任务
		auto sumTaskId = tm.add([](int x, int y)
								{ return std::to_string(x + y); },
								1, 2);
		// 添加一个成员函数任务
		auto sumTaskId2 = tm.add(&CA::memberFunc, &ca, 1, 3.4);
	}

	auto res = tm.executeAll();

	// 记录结束时间点
	auto end = std::chrono::high_resolution_clock::now();

	for (auto r : res)
	{
		std::cout << r->toString() << std::endl;
	}
	std::chrono::duration<double, std::milli> diff = end - start;
	std::cout << "Tasks took " << diff.count() << " ms to complete." << std::endl;
	return 0;
}
