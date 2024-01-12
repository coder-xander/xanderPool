

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

	// 如图想用字符串映射到任务，先定义一个容器map
	std::unordered_map<std::string, TaskIdPtr> taskIdMap;

	// 添加一个全局函数
	auto sumTaskId = tm.add([](int x, int y)
							{ std::cout<<"Sum 33333333333333333"<<std::endl;
								return std::to_string(x + y); },
							1, 2);
	taskIdMap.insert({"abababa", sumTaskId}); // 添加到map
	// 添加一个成员函数
	auto sumTaskId2 = tm.add(&ClassA::memberFunc, &ca, 1, 3.4);
	// 也可以添加全局函数
	// 如果要用abababa找到这个任务
	auto task = tm.findTask(taskIdMap["abababa"]);
	// 执行
	tm.execute(taskIdMap["abababa"]);
	// auto res = tm.executeAll(); // 全部执行

	// for (auto r : res)
	// {
	// 	std::cout << r->toString() << std::endl;
	// }
	system("pause");
	return 0;
}
