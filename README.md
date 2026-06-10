# xanderPool

[中文](README-CN.md)

![GitHub last commit](https://img.shields.io/github/last-commit/coder-xander/xanderPool)
![Language](https://img.shields.io/badge/language-C%2B%2B20-blue)
![License](https://img.shields.io/github/license/coder-xander/xanderPool)
![Header Only](https://img.shields.io/badge/header--only-yes-green)

A header-only C++20 thread pool with **work-stealing** and **structured concurrency** (TaskGroup). Pure C++ standard library, zero external dependencies — copy and use. Designed as a learning implementation of modern C++ concurrency patterns.

## Features

- **Header-only** — copy `src/` into your project, no build system needed
- **Work-stealing** — idle workers steal tasks from busy ones, auto-balancing load
- **Structured concurrency** — `TaskGroup` for scoped, cancellable task trees with exception propagation
- **Task priority** — High / Normal / Low metadata on every task
- **Dynamic scaling** — auto-creates workers under load, reclaims idle ones without deadlock
- **Static mode** — fixed worker count for deterministic behavior
- **Thread safe** — all `submit` / `submitSome` are concurrent-safe, no data races
- **Memory safe** — `shared_ptr` throughout; only manage `Pool` lifetime
- **Callable variety** — lambdas, global functions, member functions, functors
- **Async results** — `TaskResult` with blocking or timed `syncGetResult`
- **Zero dependencies** — pure C++ standard library, no external libs

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
  - [Getting Results](#getting-results)
  - [Batch Submission](#batch-submission)
  - [Structured Concurrency (TaskGroup)](#structured-concurrency-taskgroup)
  - [Task Duplication](#task-duplication)
  - [Task Priority](#task-priority)
  - [Using Workers Directly](#using-workers-directly)
  - [Singleton Mode](#singleton-mode)
- [Build & Test](#build--test)
- [Thread Safety](#thread-safety)
- [Memory Safety](#memory-safety)
- [Notes](#notes)

## Architecture

```
┌────────────────────────────────────────────────────┐
│                     Pool (Manager)                 │
│  ┌─────────────────────────────────────────────┐  │
│  │           Task Dispatch Policy              │  │
│  │  1. Idle worker with empty queue → assign   │  │
│  │  2. All busy + cap not reached → create     │  │
│  │  3. All busy + cap reached → min tasks      │  │
│  └─────────────────────────────────────────────┘  │
│                                                    │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐           │
│  │ Worker 1 │ │ Worker 2 │ │ Worker N │ (auto-    │
│  │ ┌──────┐ │ │ ┌──────┐ │ │ ┌──────┐ │  scaling) │
│  │ │Deque │ │ │ │Deque │ │ │ │Deque │ │           │
│  │ │LIFO  │ │ │ │LIFO  │ │ │ │LIFO  │ │           │
│  │ └──────┘ │ │ └──────┘ │ │ └──────┘ │           │
│  │    ↕ steal    ↕ steal     ↕ steal              │
│  └──────────┘ └──────────┘ └──────────┘           │
│        ↑           ↑           ↑                   │
│        └───────────┴───────────┘                   │
│           Work-stealing between workers            │
│                                                    │
│  Idle reclaim ── lazily removes workers idle       │
│  longer than expiry time (lock-free on shutdown)   │
└────────────────────────────────────────────────────┘

submit() / spawn() → TaskResult<T> → syncGetResult() / syncGetResult(ms)
```

Each worker owns a **single** `WorkStealingDeque` (LIFO for owner, FIFO for stealers). When a worker's local queue is empty, it randomly picks another worker and steals from the back of their deque. This ensures cache locality for the owner and fairness across workers.

## API Reference

### Creating a Pool

```cpp
// Singleton (recommended for global use)
Pool* pool = Pool::instance();

// Local instance
Pool pool;                        // dynamic, min=2, max=hardware_concurrency
Pool pool(4, 8);                  // dynamic, min=4, max=8
Pool pool(4, 8, 3000);            // + idle reclaim timeout (ms)

// Static mode (fixed worker count, no reclaim)
pool.useStaticMode();             // workers = hardware_concurrency
pool.useStaticMode(4);            // workers = 4
```

Default constructor uses `min=2, max=hardware_concurrency, idleReclaim=3000ms`. Workers idle longer than the timeout are reclaimed during the next `submit()` call.

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
auto task = makeTask([]() { return 1; });
pool.submit(task);
```

`submit` always returns a `TaskResultPtr<T>`. The task is dispatched via `pickWorker()` — idle worker first, then new worker if under capacity, then least-loaded.

### Getting Results

```cpp
auto result = pool.submit([]() { return 42; });

// Blocking wait
int value = result->syncGetResult();

// Timed wait
//   - For non-void tasks: returns std::optional<T>
//   - For void tasks: returns void (blocks up to timeout, then returns)
auto opt = result->syncGetResult(100); // 100ms timeout
if (opt.has_value()) {
    int v = opt.value();
}
```

Each `TaskResult` wraps a `std::future`. `syncGetResult` can only be called **once** — the underlying `future.get()` is single-shot.

### Batch Submission

```cpp
auto t1 = makeTask([]() { return 1; });
auto t2 = makeTask([]() { return "hello"; });

pool.submitSome({t1, t2});

// Get results from the task objects directly
int r1 = t1->getTaskResult()->syncGetResult();
```

`submitSome` has no return value (tasks may have different types). Retrieve results through individual task objects.

### Structured Concurrency (TaskGroup)

`TaskGroup` provides scoped, cancellable concurrency with automatic exception propagation:

```cpp
{
    auto group = pool.createGroup();  // NRVO, no move required

    // Spawn tasks — all run concurrently on the pool
    group.spawn([]() { /* task 1 */ });
    group.spawn([]() { /* task 2 */ });

    // Wait for all tasks to complete
    // Re-throws the first exception from any task
    group.wait();
} // destructor cancels unfinished tasks and waits for running ones
```

Key behaviors:
- **Cancellation** — calling `group.cancel()` sets a flag; tasks that haven't started yet will skip execution
- **Exception propagation** — the first exception thrown by any task is re-thrown on `wait()`
- **Destructor safety** — `~TaskGroup()` cancels pending tasks and blocks until all running tasks finish
- **Nesting** — TaskGroups can be nested arbitrarily

### Task Duplication

A task can only be submitted once. Use `copy()` for re-submission:

```cpp
pool.submit(task1);
pool.submit(task1->copy());                           // same task again
pool.submit(task2->copy()->setPriority(TaskBase::High)); // copy + boost priority
```

### Task Priority

| Priority | Enum | Notes |
|----------|------|-------|
| High | `TaskBase::High` | Stored as metadata on the task |
| Normal | `TaskBase::Normal` | Default |
| Low | `TaskBase::low` | |

**Currently**, priority is stored as **metadata** — the worker deque is a single LIFO queue and does not reorder by priority. The `PriorityOrdering` test validates the intended contract: when tasks of different priorities are submitted densely, higher-priority ones tend to execute first due to LIFO ordering of recent submissions. True priority queue ordering is planned (see `work_stealing.h`).

### Using Workers Directly

Bypass the pool for single-threaded sequential queues:

```cpp
WorkerPtr worker = Worker::makeShared();
auto result = worker->submit([]() {
    return 1 + 2;
});
result->syncGetResult(200);
```

**Caution:** a standalone Worker (no Pool) cannot steal tasks from other workers. For work-stealing, always submit through the Pool.

### Singleton Mode

```cpp
// First call creates the Pool; subsequent calls return the same instance
Pool::instance()->submit([]() { return 42; });

// Reset for testing
Pool::singletonReset();
```

`singletonReset()` destroys the singleton entirely. Thread-safe via double-checked locking.

## Build & Test

### Header-Only Usage

Copy `src/` into your project:

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
cmake .. -DCMAKE_BUILD_TYPE=Debug   # or Release
cmake --build . -j$(nproc)
ctest --output-on-failure --timeout 30
```

Tests use [Google Test](https://github.com/google/googletest) (auto-fetched by CMake via FetchContent).

### Test Coverage

| Test Suite | Tests | Description |
|-----------|-------|-------------|
| `task_submit_test` | 4 | Submit global functions, member functions, lambdas, functors |
| `task_find_test` | 1 | Find tasks by name across workers |
| `task_performance_test` | 1 | 1000 tasks, verifies all complete |
| `dev_test` | 1 | 1000 Pool create/destroy cycles |
| `test_work_stealing` | 3 | Work-stealing basic, uneven load, stress with reclaim |
| `test_task_group` | 6 | TaskGroup basic, return values, cancel, exception, nested, NRVO |
| `test_priority` | 1 | Priority ordering (single worker, LIFO-based) |
| `test_concurrent_safety` | 1 | 8 threads × 100 submits, no data races |
| `test_batch` | 1 | Batch submission via submitSome |
| `test_timeout` | 1 | syncGetResult timeout returns empty optional |
| `test_scaling` | 1 | Dynamic worker scaling min→max |
| `test_reclaim` | 2 | Deadlock regression + concurrent reclaim with submit |
| `test_deque` | 7 | WorkStealingDeque all methods, LIFO/FIFO order, concurrent push/pop/steal |
| `test_singleton` | 3 | Singleton instance consistency, reset, concurrent access |
| `test_pool_config` | 5 | Static mode, min==max, query API, custom expiry |
| `test_task_ext` | 10 | Task copy, void tasks, priority metadata, pre-made task, task name |
| **Total** | **56** | One file per topic, each independently runnable |

## Thread Safety

- All `submit()` and `submitSome()` variants are thread-safe
- Worker management (creation, reclaim) is internally synchronized with exclusive + shared mutex
- `stealFromRandomWorker` holds a shared (read) lock, allowing concurrent stealing; worker creation/reclaim takes an exclusive lock
- `dumpWorkers()` is safe from any thread
- **Deadlock-free reclamation:** idle worker shutdown is deferred outside the pool mutex to avoid lock ordering inversion with concurrent stealing

## Memory Safety

- Core objects (`Pool`, `Worker`, `Task`, `TaskResult`) managed via `shared_ptr`
- Only manage the `Pool` lifetime; worker threads are joined automatically in destructor
- `asyncDestroyed()` fires shutdown on all workers asynchronously and returns a `future<bool>`

## Telemetry

`Pool::dumpWorkers()` returns a formatted table of all workers with runtime counters. Useful for debugging load balance and worker activity.

```cpp
// Call from any thread at any time
std::cout << pool.dumpWorkers() << std::endl;

// Example output:
// +-------------------+------+--------+-------+--------+------+------+
// | Thread ID         | Tasks| State  | Steals| Stolen | Idle | Exec |
// +-------------------+------+--------+-------+--------+------+------+
// | 0x7f8c9a0b7640    |    0 | Idle   |    12 |     3  |    5 |  150 |
// | 0x7f8c9a0b7040    |    1 | Busy   |     8 |     5  |    2 |   89 |
// +-------------------+------+--------+-------+--------+------+------+
```

| Column | Counter | Description |
|--------|---------|-------------|
| Tasks | `taskCount()` | Tasks currently in this worker's local deque |
| State | `state()` | `Idle`, `Busy`, or `Stopped` |
| Steals | `stealAttempts_` | Times this worker tried to steal from others |
| Stolen | `tasksStolenAway_` | Times a task was stolen from this worker by another |
| Idle | `idleWaits_` | Times this worker entered idle wait (condition variable) |
| Exec | `tasksExecuted_` | Tasks actually executed by this worker |

The counters are `std::atomic<uint64_t>` and reset on Worker creation. Safe to call from any thread (holds a shared lock on the worker list).

## Notes

1. A `Task` can only be submitted once — use `copy()` for re-submission
2. A `TaskResult` can only be read once — `syncGetResult` is single-shot (wraps `std::future::get`)
3. Pool destructor blocks until in-flight tasks complete; use `asyncDestroyed()` for non-blocking shutdown
4. `TaskGroup` is **non-movable** (`std::counting_semaphore` is not movable) — always use `createGroup()` which benefits from guaranteed copy elision (NRVO) in C++17
5. Priority is metadata only; true priority-ordered queues are planned
