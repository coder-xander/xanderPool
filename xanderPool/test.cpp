#include <deque>
#include <future>
#include "xanderPool/include/pool.h"
#include "xanderPool/include/worker.h"
#include "tool.h"
#include "xanderPool/include/task.h"
using namespace std;
using namespace xander;


long long globalFibFunction(int n)
{
    if (n <= 1)
        return n;
    else
        return globalFibFunction(n - 1) + globalFibFunction(n - 2);
}

int main()
{
    class ClassA
    {
    public:
        std::string memberFunction(int a, double b)
        {
            this_thread::sleep_for(std::chrono::milliseconds(300));
            return  std::to_string(a) + std::to_string(b);
        }
    };
    ClassA aobj;
    auto lambdaFunction = []()
        {
            printf_s("hello ,xander \n");

        };

    Pool pool1 = Pool(3, 8);


    pool1.submit(lambdaFunction);//submit lambda function
    pool1.submit(globalFibFunction, 12);//submit global function
    pool1.submit(&ClassA::memberFunction, &aobj, 1, 2);//submit member function

    //pre build
    auto task1 = makeTask(TaskBase::Normal, []() {});
    auto task2 = makeTask(TaskBase::Normal, globalFibFunction, 12);
    auto task3 = makeTask(TaskBase::Normal, &ClassA::memberFunction, ClassA(), 1, 2);
    pool1.submit(task1);
    pool1.submit(task2);
    pool1.submit(task3);
    //submit batch
    pool1.submitSome({ task1->copy() ,task1->copy()->setPriority(TaskBase::High) });

    std::cout << pool1.dumpWorkers() << std::endl;





















    timeTest("添加任务", [&]() mutable
        {
        });
    system("pause");
    return 0;

}
