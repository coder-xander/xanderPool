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
    {

        return n;
    }
    else
    {
        return globalFibFunction(n - 1) + globalFibFunction(n - 2);
    }
}

int main()
{
    auto s = globalFibFunction(2);
http: // gitlab.ciqtek.com/epr/EPR200M_2.0/issues
    class ClassA
    {
    public:
        // 成员函数
        std::string memberFunction(int a, double b)
        {
            this_thread::sleep_for(std::chrono::milliseconds(300));
            return std::to_string(a) + std::to_string(b);
        }
        // 伪函数
        std::string operator()()
        {
            printf_s("std::string operator() called \n");
            return "ok";
        }
    };
    ClassA aobj;
    auto lambdaFunction = []()
        {
            printf_s("hello ,xander \n");
        };
    //----------------------------------------
    auto asyncResult1 = Pool::instance()->submit([](double a, double b)
        { return pow(a, b); },
        1.2, 3);
    double restlt = asyncResult1->syncGetResult(); //
    //--------------------------------------------------------------------------------------------
    Pool pool1 = Pool();
    pool1.useStaticMode();
    pool1.submit(lambdaFunction);                       // submit lambda function
    pool1.submit(globalFibFunction, 12);                // submit global function
    pool1.submit(&ClassA::memberFunction, &aobj, 1, 2); // submit member function
    //--------------------------------------------------------------------------------------------
    auto task1 = makeTask(TaskBase::Normal, []() {});
    auto task2 = makeTask(TaskBase::Normal, globalFibFunction, 12);
    auto task3 = makeTask(TaskBase::Normal, &ClassA::memberFunction, ClassA(), 1, 2);
    auto task4 = makeTask(aobj);
    task4->setName("AAA");

    pool1.submit(task1);
    pool1.submit(task2);
    pool1.submit(task3);
    pool1.submit(task4);
    auto sharedPtrs = pool1.findTasks("AAA");

    //--------------------------------------------------------------------------------------------
    pool1.submitSome({ task1->copy(), task1->copy()->setPriority(TaskBase::High) });
    // directly submit
    pool1.submit(TaskBase::Normal, globalFibFunction, 12);
    pool1.submit(TaskBase::High, &ClassA::memberFunction, &aobj, 1, 2);
    pool1.submit(aobj);
    std::cout << pool1.dumpWorkers() << std::endl;

    // submit some//--------------------------------------------------------------------------------------------
    auto task11 = makeTask([]()
        {
            std::cout << "task11" << std::endl;
            return 1; });
    auto task12 = makeTask([]()
        {
            std::cout << "task12" << std::endl;

            return "ssss"; });
    pool1.submitSome({ task11, task12 });
    pool1.submitSome({ task11->copy(), task12->copy()->setPriority(TaskBase::High) });
    int task11Result = task11->getTaskResult()->syncGetResult();
    // performance  test--------------------------------------------------------------------------------------------
    std::vector<TaskResultPtr<TaskBase::Priority>> asyncResult;
    for (int i = 0; i < 1000; ++i)
    {
        auto r = pool1.submit([]()
            {
                printf_s("hello ,I am a worker\n");
                return randomPriority(); });
        asyncResult.push_back(r);
    }
    std::vector<TaskBase::Priority> results;
    for (auto r : asyncResult)
    {
        auto res = r->syncGetResult();
        results.push_back(res);
    }
    timeTest("添加任务", [&]() mutable {});

    //--------------------------------------------------------------------------------------------------------------
    system("pause");
    return 0;
}