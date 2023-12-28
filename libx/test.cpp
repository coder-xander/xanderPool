
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

	TaskManager tm;
	CA ca;
	// 添加一个全局函数任务
	auto sumTaskId = tm.add([](int x, int y)
							{ return std::to_string(x + y); },
							1, 2);
	// 添加一个成员函数任务
	auto sumTaskId2 = tm.add(&CA::memberFunc, &ca, 1, 3.4);
	auto res = tm.executeAll();

	for (auto r : res)
	{
		std::cout << r->toString() << std::endl;
	}

	return 0;
}
