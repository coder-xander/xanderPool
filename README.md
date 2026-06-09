# xanderPool

[中文](README-CN.md)

![GitHub last commit](https://img.shields.io/github/last-commit/coder-xander/xanderPool)
![Language](https://img.shields.io/badge/language-C%2B%2B20-blue)
![License](https://img.shields.io/github/license/coder-xander/xanderPool)
![Header Only](https://img.shields.io/badge/header--only-yes-green)

A high-performance, cross-platform, header-only C++20 thread pool with task priority support. Modern C++ design, zero external dependencies — just copy and use.

## Features

- **Header-only** — copy `src/` into your project, no build system needed
- **Task priority** — High / Normal / Low, always executes highest priority first
- **Dynamic scaling** — auto-creates workers under load, reclaims idle ones
- **Static mode** — fixed worker count when you need deterministic behavior
- **Memory safe** — `shared_ptr` everywhere, only manage Pool lifetime
- **Thread safe** — all `submit` / `submitSome` are concurrent-safe
- **Callable variety** — lambdas, global functions, member functions, functors
- **Async results** — `TaskResult` with blocking or timed `syncGetResult`
- **Zero dependencies** — pure C++ standard library

## Quick Start

```cpp
#include "pool.h"
#include <cmath>

using namespace xander;

// Submit a lambda, get an async result
auto result = Pool::instance()->submit([](double a, double b) {
    return pow(a, b);
}, 1.2, 3);

double value = result->syncGetResult(); // 1.728
```

## Table of Contents

- [Architecture](#architecture)
- [API Reference](#api-reference)
  - [Creating a Pool](#creating-a-pool)
  - [Submitting Tasks](#submitting-tasks)
  - [Task Priority](#task-priority)
  - [Getting Results](#getting-results)
  - [Batch Submission](#batch-submission)
  - [Task Duplication](#task-duplication)
  - [Using Workers Directly](#using-workers-directly)
- [Build & Test](#build--test)
- [Performance](#performance)
- [Thread Safety](#thread-safety)
- [Memory Safety](#memory-safety)
- [Notes](#notes)
- [Contact](#contact)

## Architecture

```
┌─────────────────────────────────────────────────┐
│                   Pool (Manager)                │
│  ┌──────────────────────────────────────────┐   │
│  │  Task Dispatch Policy                    │   │
│  │  1. Idle worker → assign                 │   │
│  │  2. All busy + cap not reached → create  │   │
│  │  3. All busy + cap reached → min tasks   │   │
│  └──────────────────────────────────────────┘   │
│                                                 │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐           │
│  │ Worker 1│ │ Worker 2│ │ Worker N│  (auto-   │
│  │ ┌─────┐ │ │ ┌─────┐ │ │ ┌─────┐ │  scaling) │
│  │ │High │ │ │ │High │ │ │ │High │ │           │
│  │ │Norm │ │ │ │Norm │ │ │ │Norm │ │           │
│  │ │Low  │ │ │ │Low  │ │ │ │Low  │ │           │
│  │ └─────┘ │ │ └─────┘ │ │ └─────┘ │           │
│  └─────────┘ └─────────┘ └─────────┘           │
│                                                 │
│  GC Thread ── periodically reclaims idle workers│
└─────────────────────────────────────────────────┘

submit() → TaskResult<T> → syncGetResult() / syncGetResult(ms)
```

## API Reference

### Creating a Pool

```cpp
// Singleton (recommended for global use)
Pool* pool = Pool::instance();

// Local instance
Pool pool;                        // dynamic, min=2, max=hardware_concurrency
Pool pool(4, 8);                  // dynamic, min=4, max=8
Pool pool(4, 8, 3000);            // + custom GC interval (ms)

// Static mode (fixed worker count)
pool.useStaticMode();             // workers = hardware_concurrency
pool.useStaticMode(4);            // workers = 4
```

### Submitting Tasks

```cpp
// Lambda
auto r1 = pool.submit([]() { return 42; });

// Lambda with arguments
auto r2 = pool.submit([](int a, int b) { return a + b; }, 3, 4);

// Global function
auto r3 = pool.submit(globalFibFunction, 12);

// Member function
ClassA obj;
auto r4 = pool.submit(&ClassA::memberFunction, &obj, 1, 2);

// Functor
auto r5 = pool.submit(obj);  // calls obj.operator()()

// With explicit priority
auto r6 = pool.submit(TaskBase::High, []() { return 1; });

// Pre-made task
auto task = makeTask(TaskBase::Normal, []() { return 1; });
pool.submit(task);
```

### Task Priority

| Priority | Enum | Behavior |
|----------|------|----------|
| High | `TaskBase::High` | Executed first |
| Normal | `TaskBase::Normal` | Default |
| Low | `TaskBase::low` | Executed last |

Each worker maintains three separate queues. The worker always picks from the highest-priority non-empty queue.

### Getting Results

```cpp
auto result = pool.submit([]() { return 42; });

// Blocking wait
int value = result->syncGetResult();

// Timed wait (returns std::optional)
auto opt = result->syncGetResult(100); // 100ms timeout
if (opt.has_value()) {
    int v = opt.value();
}
```

### Batch Submission

```cpp
auto t1 = makeTask([]() { return 1; });
auto t2 = makeTask([]() { return "hello"; });

pool.submitSome({t1, t2});

// Get results from the task objects themselves
int r1 = t1->getTaskResult()->syncGetResult();
```

`submitSome` has no return value because tasks may have different types. Use the task object to retrieve its result.

### Task Duplication

A task can only be submitted once. Use `copy()` to re-submit:

```cpp
pool.submit(task1);
pool.submit(task1->copy());                           // same task again
pool.submit(task2->copy()->setPriority(TaskBase::High)); // copy + boost priority
```

### Using Workers Directly

Bypass the pool for single-threaded task queues:

```cpp
WorkerPtr worker = Worker::makeShared();
auto result = worker->submit([]() {
    return 1 + 2;
});
result->syncGetResult(200);
```

## Build & Test

### Header-Only Usage

Copy the `src/` directory into your project:

```bash
cp -r src/ /your/project/include/xanderPool/
```

```cpp
#include "xanderPool/pool.h"
```

Requires C++20 (`-std=c++20`).

### Building Tests

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
ctest --timeout 30
```

Tests use [Google Test](https://github.com/google/googletest) (auto-fetched by CMake).

### Test Coverage

| Test Suite | Description |
|-----------|-------------|
| `task_submit_test` | Submit global functions, member functions, lambdas, functors |
| `task_find_test` | Find tasks by name across workers |
| `task_performance_test` | 1000 tasks, verifies all complete |
| `dev_test` | 1000 Pool create/destroy cycles |
| `comprehensive_test` | Priority ordering, concurrent submit, timeout, batch submit, dynamic scaling |

## Performance

100,000 empty tasks submitted on a 10th-gen i5 (Release build):

```
~135ms to enqueue all tasks
Workers auto-scale to handle the burst
```

The dynamic scaling + GC reclamation keeps resource usage efficient under varying load.

## Thread Safety

- All `submit()` and `submitSome()` variants are thread-safe
- Worker management (creation, GC reclamation) is internally synchronized
- `dumpWorkers()` is safe to call from any thread

## Memory Safety

- Core objects (`Pool`, `Worker`, `Task`, `TaskResult`) are managed via `shared_ptr`
- You only need to manage the `Pool` lifetime
- Pool destructor completes all in-flight tasks before shutdown
- `asyncDestroyed()` for non-blocking shutdown

## Notes

1. A `Task` can only be submitted once — use `copy()` for re-submission
2. A `TaskResult` can only be read once — calling `syncGetResult` twice is undefined
3. Pool destructor blocks until in-flight tasks complete

## Contact

Email: xhr1028@foxmail.com
