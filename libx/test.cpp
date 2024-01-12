

#include <future>

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
		std::cout << "Sum memberFunc" << std::endl;
		return std::to_string(a) + std::to_string(b);
	}
};
int main()
{

	TaskManager tm;
	ClassA ca;
	// 添加一个全局函数
	std::vector<std::future<void >> fs;

	for (int i = 0; i < 12; ++i)
	{
		auto f = std::async(std::launch::async, [&,i]()
			{
				for (int j = 0; j < 1000; ++j)  
				{
					auto sumTaskId = tm.add([](int x, int y)
						{ std::cout << "Sum 33333333333333333" << std::endl;
					return std::to_string(x + y); },
						1, 2);
					// 添加一个成员函数
					auto sumTaskId2 = tm.add(&ClassA::memberFunc, &ca, 1, 3.4);
					std::cout << "add num" << std::to_string(i) << ":" << std::to_string(j) << std::endl;

				}
			});

		fs.push_back(std::move(f));
	}
	for (auto& future : fs)
	{
		future.get();
	}
	
	// 也可以添加全局函数
	auto res = tm.executeAll(); // 全部执行
	for (auto r : res)
	{
		std::cout << r->toString() << std::endl;
	}
	std::cout << "res size:" << std::to_string(res.size()) << std::endl;

	system("pause");
	return 0;
}
