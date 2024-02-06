# xanderPool Introduction

[中文](README-CN.md)

![GitHub last commit](https://img.shields.io/github/last-commit/coder-xander/xanderPool) 

xanderPool is a user-friendly, high-performance, cross-platform, automatic memory-managed, thread-safe, header-only, C++ 17 thread pool based on task priority. It aims to be a simple yet efficient thread pool with various ways to submit tasks, offering clear and straightforward design patterns and logic. It follows modern C++ coding styles.

The thread pool is non-static and the number of threads is dynamically adjustable.

Built on the C++ standard library without relying on any other libraries, it's easy to integrate. You can get started quickly and efficiently utilize system thread resources by submitting task objects.

Usage: Since xanderPool is header-only, it means you can just copy the xanderPool/include directory into your project to use it.

## Quickly Submit a Task and Get Results

```cpp
// Use Pool singleton to submit an anonymous lambda task to calculate 1.2 raised to the power of 3, returning a TaskResult asynchronous object
auto asyncResult1 = Pool::instance()->submit([](double a, double b)
{
    return pow(a, b);

}, 1.2, 3);
// syncGetResult blocks here until the task completes
double result = asyncResult1->syncGetResult(); // 1.7279999999999
```

It's very straightforward - xanderPool uses the submit function to submit tasks and uses the asynchronous result TaskResult to obtain results. At this point, you can make normal use of xanderPool. Just keep using the provided singleton object to add various tasks as needed.

# Detailed Introduction

#### Several Concepts of xanderPool

1. Pool: Accepts tasks and allocates them to workers, managing the workers.
2. Task: A wrapper for callable objects.
3. Worker: The task handler.
4. Task Result (taskResult)

![1707112692969](image/README/1707112692969.png)

You just need to create a Pool instance, submit tasks, and you'll get the task results. Afterward, the Pool will automatically distribute your tasks and manage the workers, executing them and retrieving results through the task result.

#### Submit Various Callable Objects:

1. Global function

```cpp
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
```

2. Member function and functor

```cpp
class ClassA
{
public:
    // Member function
    std::string memberFunction(int a, double b)
    {
        this_thread::sleep_for(std::chrono::milliseconds(300));
        return std::to_string(a) + std::to_string(b);
    }
    // Functor
    std::string operator()()
    {
        printf_s("fake function called \n");
        return "fake func";
    }
};
ClassA aobj;
```

3. Lambda function

```cpp
auto lambdaFunction = []()
{
    printf_s("hello, xander \n");
};
```

##### Submit Anonymously Using Pool

```cpp
Pool pool1;
pool1.submit(TaskBase::low, []() {});
pool1.submit(TaskBase::Normal, globalFibFunction, 12);
pool1.submit(TaskBase::High, &ClassA::memberFunction, &aobj, 1, 2);
pool1.submit(aobj);
```

##### Non-anonymous Submission (Construct with makeTask First)

```cpp
auto task1 = makeTask(TaskBase::Normal, []() {});
auto task2 = makeTask(TaskBase::High, globalFibFunction, 12);
auto task3 = makeTask(TaskBase::Low, &ClassA::memberFunction, ClassA(), 1, 2);
auto task4 = makeTask(aobj);
pool1.submit(task1);
pool1.submit(task2);
pool1.submit(task3);
pool1.submit(task4);
```

xanderPool can submit almost all types of callable objects, allowing tasks to be created and submitted anywhere. Please use the global function makeTask to create any type of task.

##### Batch Submission

The Pool's submitSome function accepts a set of tasks.

```cpp
auto task11 = makeTask([]()
{
    std::cout << "task11" << std::endl;
    return 1;
});

auto task12 = makeTask([]()
{
    std::cout << "task12" << std::endl;
    return "ssss";
});

pool1.submitSome({ task11, task12 });
int task11Result = task11->getTaskResult()->syncGetResult();
```

There is no return value for this function. If you want to get results, you can do so using the Task itself to find the results (through the getTaskResult function). The reason for this design is that task11 and task12 may have different return values, so should the submitSome function return a tuple? Considering the hassle of using std::get to retrieve values from a tuple, it's more direct to use the task to get its own result object.

##### Task Duplication

The same Task can only be submitted to a Worker once. If you want to execute the same task multiple times, the task provides a copy function for convenient duplication.

```cpp
pool1.submitSome({ task11, task12 });
pool1.submitSome({ task11->copy(), task12->copy()->setPriority(TaskBase::High)});
```

The task11 and task12 were duplicated, and the priority of task12 was raised to High for re-execution.

#### Getting Task Results

Except for batch addition of tasks where the task itself needs to obtain results (see Batch Submission), all other submit functions return a TaskResultPtr, which wraps the asynchronous result of the return value. An example is demonstrated next.

```cpp
std::vector<TaskResultPtr<TaskBase::Priority>> asyncResult; // Asynchronous results
for (int i = 0; i < 1000; ++i)
{
    auto r = pool1.submit([this]()
    {
        return randomPriority();
    });
    asyncResult.push_back(r);
}
std::vector<TaskBase::Priority> results; // Results
for (auto r : asyncResult)
{
    auto res = r->syncGetResult();
    results.push_back(res);
}
```

A thousand tasks were submitted to get a thousand random priorities. Async results are stored in an asynchronous container, then syncGetResult is used to get the results and put them in a container. The syncGetResult function can take a time argument in ms, like r->syncGetResult(100); this means it will block and wait for the result, with a timeout of 100ms. If not specified, it will wait indefinitely until the result is obtained.

#### Task Priority

xanderPool has a simple task priority system.

Priority levels:

1. High priority: TaskBase::High
2. Normal priority, which is also the default: TaskBase::Normal
3. Low priority: TaskBase::Low

All of Pool's submit and the global makeTask function's first parameter can be the priority level. If not specified, the default is normal priority.

Logic: All workers managed by the Pool have three queues for three different priority levels. The threads owned by the worker always check from high to low priority, always executing the highest priority task in the queue first.

#### Automatic Adjustment of Workers

xanderPool is not a static thread pool. It can dynamically create and recycle resources.

##### Creation

When a Pool is created, you can specify the minimum and maximum number of workers in the constructor. When the Pool is created, it will create the minimum number of workers. When the Pool's submit function is called, it will dynamically increase the number of workers, but not exceed the maximum limit. If not specified, 2 workers will be created initially, and during the subsequent dynamic increase, no more than the number of CPU cores will be created.

##### Recycling

A worker owns a thread used to execute tasks assigned to it. When there are few tasks, idle workers with threads can consume resources. xanderPool implements a simple resource reclaimer that scans workers at intervals to recycle idle workers.

Logic: When the Pool is created, a resource reclamation thread is started, which monitors workers at set intervals. This time is by default 5 seconds but can be set with the Pool's member function setWorkerExpiryTime.

##### Task Allocation Strategy

When the Pool's submit function receives a task, it will decide which worker to assign the task to.

Logic: If there are idle workers at the time of decision-making, the task is given to an idle worker. If all workers are busy and the number of workers has not reached the maximum; a worker is created, and the task is assigned to it. If the maximum number of workers is reached, the task is assigned to the worker with the fewest tasks.

This strategy, combined with resource reclamation, achieves automatic adjustment of workers.

#### Performance

xanderPool has very high performance. Use the timeTest function provided by tool.h to test the time it takes to submit 100,000 empty tasks in Release:

CPU: 10th Gen i5

```cpp
timeTest("Finished adding", [&pool1]()
{
    for (int i = 0; i < 100000; i++)
    {
        pool1.submit([](){});
    }
});
```

Console output:

![1707104137488](image/README/1707104137488.png)

It took 135ms. Many "add worker" prints indicate that the Pool is dynamically increasing the number of workers to cope with a large number of tasks.

By the way, if you want to see the workers managed by the Pool, you can use the Pool's dumpWorkers function. As shown in the chart above, it shows the current workers and the unfinished tasks they have. You can see that by the time the tasks are added, the workers have almost finished these tasks, indicating the high task consumption and production coherence of xanderPool.

#### Memory Safety

##### Smart Pointers

At the core of xanderPool: Task, Pool, TaskResult, Worker in actual code usually manifest as TaskPtr, PoolPtr, TaskResultPtr..., these are aliases for std::shared_ptr. xanderPool makes extensive use of smart pointers to ensure memory safety; you only need to focus on the lifecycle of the Pool.

##### Asynchronous Destruction of Pool

If the Pool is destructed while there are still unfinished tasks, it will cause all workers to abandon the incomplete tasks but will continue to complete tasks that are already in execution before the destruction occurs. So there might be blocking, which is normal.

```cpp
~Pool()
{
    const auto f = asyncDestroyed();
    f.wait();
}
```

As in the wait point above.

Since there is an asyncDestroyed function, if you want to destruct a Pool with ongoing tasks without blocking, you can manually call the Pool's asyncDestroyed function.

#### Using the Worker Independently

The worker is a thread owner, which means you can directly create a Worker and submit tasks to it directly, bypassing the Pool, like this:

```cpp
WorkerPtr worker = Worker::makeShared();
auto result3 = worker->submit([]()
{
    printf_s("hello, I am a worker\n");
    return 1 + 2;
});
result3->syncGetResult(200);
```

You can keep this worker safely, and now you have a worker dedicated to working for you.

So why not submit directly to the Pool? As previously mentioned, the Pool will have a task distribution strategy, and you won't know which worker your task has been assigned to. If your requirement is for many tasks to be executed in a single thread, you should create a worker and submit these tasks to it.

Thus, with xanderPool, whether it is single-threaded creation and use or thread pool usage, the API is uniform.

#### Thread Safety

All submit functions and submitSome functions are thread-safe.

#### Attention Points

1. The same Task can only be submitted once. If duplication is needed, use its copy member function.
2. A TaskResult can only get a result once; attempting to do so again will result in an error.
