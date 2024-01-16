

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
	for (int j = 0; j < 10000; ++j)
	{
		auto r1 = xpoPool.acceptTask([j](int x, int y)
			{ std::cout << "run lambda !" << std::endl;
		return "lamda add res num :" + std::to_string(j); },
			1, 2);
		// 添加一个成员函数
		auto r2 = xpoPool.acceptTask(&ClassA::memberFunc, &ca, 1, 3.4);
		results.push_back(r1);
		results.push_back(r2);
	}
	// 也可以添加全局函数

	for (auto r : results)
	{
		std::cout << "获得结果：" << r->toString() << std::endl;
	}
	std::cout << "res size:" << std::to_string(results.size()) << std::endl;

	system("pause");
	return 0;
}
