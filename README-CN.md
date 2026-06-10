# xanderPool

[English](README.md)

![GitHub last commit](https://img.shields.io/github/last-commit/coder-xander/xanderPool)
![Language](https://img.shields.io/badge/language-C%2B%2B20-blue)
![License](https://img.shields.io/github/license/coder-xander/xanderPool)
![Header Only](https://img.shields.io/badge/header--only-yes-green)

Header-only 的 C++20 线程池，支持 **工作窃取（work-stealing）** 和 **结构化并发（TaskGroup）**。纯 C++ 标准库实现，零外部依赖，拷贝即用。基于现代 C++ 并发模式的学习实现。

## 特性

- **Header-only** — 拷贝 `src/` 到项目即可，无需构建系统
- **工作窃取** — 空闲 worker 自动从忙碌 worker 窃取任务，均衡负载
- **结构化并发** — `TaskGroup` 提供作用域内可取消的任务树，异常自动传播
- **任务优先级** — 高/普通/低，每个任务携带优先级元数据
- **动态扩缩容** — 高负载自动创建 worker，空闲自动回收（无死锁）
- **静态模式** — 固定 worker 数量，适合确定性场景
- **线程安全** — 所有 `submit` / `submitSome` 并发安全，无数据竞争
- **内存安全** — 全程 `shared_ptr`，只需管理 Pool 生命周期
- **多种可调用对象** — lambda、全局函数、成员函数、仿函数
- **异步结果** — `TaskResult` 支持阻塞等待和超时等待
- **零依赖** — 纯 C++ 标准库，无需第三方库

## 快速上手

```cpp
#include "pool.h"
#include <cmath>

using namespace xander;

// 提交 lambda，获取异步结果
auto result = Pool::instance()->submit([](double a, double b) {
    return pow(a, b);
}, 1.2, 3);

double value = result->syncGetResult(); // 1.728
```

## 目录

- [架构](#架构)
- [API 参考](#api-参考)
  - [创建 Pool](#创建-pool)
  - [提交任务](#提交任务)
  - [获取结果](#获取结果)
  - [批量提交](#批量提交)
  - [结构化并发（TaskGroup）](#结构化并发-taskgroup)
  - [任务复制](#任务复制)
  - [任务优先级](#任务优先级)
  - [直接使用 Worker](#直接使用-worker)
  - [单例模式](#单例模式)
- [构建与测试](#构建与测试)
- [线程安全](#线程安全)
- [内存安全](#内存安全)
- [注意事项](#注意事项)

## 架构

```
┌────────────────────────────────────────────────────┐
│                     Pool（管理器）                   │
│  ┌─────────────────────────────────────────────┐  │
│  │           任务分配策略                       │  │
│  │  1. 有空闲 worker + 空队列 → 直接分配       │  │
│  │  2. 全忙 + 未达上限 → 创建新 worker         │  │
│  │  3. 全忙 + 已达上限 → 分给任务最少的        │  │
│  └─────────────────────────────────────────────┘  │
│                                                    │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐           │
│  │ Worker 1 │ │ Worker 2 │ │ Worker N │ (自动     │
│  │ ┌──────┐ │ │ ┌──────┐ │ │ ┌──────┐ │  扩缩)    │
│  │ │Deque │ │ │ │Deque │ │ │ │Deque │ │           │
│  │ │LIFO  │ │ │ │LIFO  │ │ │ │LIFO  │ │           │
│  │ └──────┘ │ │ └──────┘ │ │ └──────┘ │           │
│  │    ↕ steal   ↕ steal     ↕ steal               │
│  └──────────┘ └──────────┘ └──────────┘           │
│        ↑           ↑           ↑                   │
│        └───────────┴───────────┘                   │
│           Worker 间工作窃取                         │
│                                                    │
│  空闲回收 ── 超过过期时间的 worker 在下次 submit     │
│  时惰性移除（shutdown 在锁外执行，防止死锁）         │
└────────────────────────────────────────────────────┘

submit() / spawn() → TaskResult<T> → syncGetResult() / syncGetResult(ms)
```

每个 worker 拥有**一个** `WorkStealingDeque`（LIFO 给 owner，FIFO 给 stealer）。当本地队列为空时，随机选择另一个 worker 从其队尾窃取任务。owner 端 LIFO 保证缓存局部性，stealer 端 FIFO 保证公平。

## API 参考

### 创建 Pool

```cpp
// 单例模式（全局推荐）
Pool* pool = Pool::instance();

// 局部实例
Pool pool;                        // 动态，min=2, max=CPU核心数
Pool pool(4, 8);                  // 动态，min=4, max=8
Pool pool(4, 8, 3000);            // + 空闲回收超时（毫秒）

// 静态模式（固定 worker 数量，不回收）
pool.useStaticMode();             // worker = CPU核心数
pool.useStaticMode(4);            // worker = 4
```

默认构造使用 `min=2, max=CPU核心数, idleReclaim=3000ms`。空闲超过超时时间的 worker 会在下次 `submit()` 时被回收。

### 提交任务

```cpp
// Lambda
auto r1 = pool.submit([]() { return 42; });

// 带参数的 Lambda
auto r2 = pool.submit([](int a, int b) { return a + b; }, 3, 4);

// 全局函数
auto r3 = pool.submit(globalFibFunction, 12);

// 成员函数
ClassA obj;
auto r4 = pool.submit(&ClassA::memberFunction, &obj, 1, 2);

// 仿函数
auto r5 = pool.submit(obj);  // 调用 obj.operator()()

// 指定优先级
auto r6 = pool.submit(TaskBase::High, []() { return 1; });

// 预创建的任务
auto task = makeTask([]() { return 1; });
pool.submit(task);
```

`submit` 始终返回 `TaskResultPtr<T>`。任务通过 `pickWorker()` 分配 — 优先空闲 worker，其次创建新 worker（未达上限），最后选择任务最少的 worker。

### 获取结果

```cpp
auto result = pool.submit([]() { return 42; });

// 阻塞等待
int value = result->syncGetResult();

// 带超时
//   - 非 void 任务：返回 std::optional<T>
//   - void 任务：返回 void（阻塞至超时或完成，然后返回）
auto opt = result->syncGetResult(100); // 100ms 超时
if (opt.has_value()) {
    int v = opt.value();
}
```

每个 `TaskResult` 包装一个 `std::future`。`syncGetResult` **只能调用一次**（底层 `future.get()` 为单次调用）。

### 批量提交

```cpp
auto t1 = makeTask([]() { return 1; });
auto t2 = makeTask([]() { return "hello"; });

pool.submitSome({t1, t2});

// 通过 task 对象获取结果
int r1 = t1->getTaskResult()->syncGetResult();
```

`submitSome` 无返回值（任务类型可能不同），通过 task 对象获取各自的 TaskResult。

### 结构化并发（TaskGroup）

`TaskGroup` 提供作用域内的可取消并发，异常自动传播：

```cpp
{
    auto group = pool.createGroup();  // NRVO，无需 move

    // 并发提交任务（都在 pool 上执行）
    group.spawn([]() { /* 任务 1 */ });
    group.spawn([]() { /* 任务 2 */ });

    // 等待所有任务完成
    // 如果有异常，重新抛出第一个
    group.wait();
} // 析构函数取消未完成任务，等待正在运行的任务
```

关键行为：
- **取消** — `group.cancel()` 设置标志；尚未开始的任务跳过执行
- **异常传播** — 任意任务抛出的第一个异常在 `wait()` 时重新抛出
- **析构安全** — `~TaskGroup()` 取消待办任务，阻塞直到所有运行中任务完成
- **嵌套** — TaskGroup 支持任意层级嵌套

### 任务复制

同一个 Task 只能提交一次，再次提交需要 `copy()`：

```cpp
pool.submit(task1);
pool.submit(task1->copy());                                // 再次执行
pool.submit(task2->copy()->setPriority(TaskBase::High));   // 复制 + 提升优先级
```

### 任务优先级

| 优先级 | 枚举值 | 说明 |
|--------|--------|------|
| 高 | `TaskBase::High` | 作为元数据存储在任务上 |
| 普通 | `TaskBase::Normal` | 默认 |
| 低 | `TaskBase::low` | 最后执行 |

**当前**，优先级仅作为**元数据**存储——worker 的 deque 是单一 LIFO 队列，不按优先级重排序。`PriorityOrdering` 测试验证的是预期契约：密集提交不同优先级任务时，由于 LIFO 特性，高优先级任务往往先被执行。真正的优先级队列排序在计划中。

### 直接使用 Worker

绕过 Pool，使用单线程顺序队列：

```cpp
WorkerPtr worker = Worker::makeShared();
auto result = worker->submit([]() {
    return 1 + 2;
});
result->syncGetResult(200);
```

**注意：** 独立 Worker（无 Pool）无法从其他 worker 窃取任务。需要工作窃取时始终通过 Pool 提交。

### 单例模式

```cpp
// 首次调用创建 Pool，后续返回同一个实例
Pool::instance()->submit([]() { return 42; });

// 重置（测试用）
Pool::singletonReset();
```

`singletonReset()` 彻底销毁单例，通过双重检查锁定保证线程安全。

## 构建与测试

### Header-Only 用法

将 `src/` 拷贝到项目中：

```bash
cp -r src/ /your/project/include/xanderPool/
```

```cpp
#include "xanderPool/pool.h"
```

需要 C++20 支持（`-std=c++20`）。

### 构建测试

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug   # 或 Release
cmake --build . -j$(nproc)
ctest --output-on-failure --timeout 30
```

测试依赖 [Google Test](https://github.com/google/googletest)（CMake 通过 FetchContent 自动拉取）。

### 测试覆盖

| 测试套件 | 测试数 | 说明 |
|---------|-------|------|
| `task_submit_test` | 4 | 全局函数、成员函数、lambda、仿函数提交 |
| `task_find_test` | 1 | 按名称在 worker 中查找任务 |
| `task_performance_test` | 1 | 1000 个任务，验证全部完成 |
| `dev_test` | 1 | 1000 次 Pool 创建/销毁循环 |
| `test_work_stealing` | 3 | work-stealing 基础、不均衡负载、带回收的压力测试 |
| `test_task_group` | 6 | TaskGroup 基础、返回值收集、取消、异常、嵌套、NRVO |
| `test_priority` | 1 | 优先级排序（单 worker，LIFO 特性） |
| `test_concurrent_safety` | 1 | 8 线程 × 100 提交，无数据竞争 |
| `test_batch` | 1 | submitSome 批量提交 |
| `test_timeout` | 1 | syncGetResult 超时返回空 optional |
| `test_scaling` | 1 | 动态扩缩容 min→max |
| `test_reclaim` | 2 | 死锁回归 + 并发提交中回收 |
| `test_deque` | 7 | WorkStealingDeque 全部方法、LIFO/FIFO 顺序、并发 push/pop/steal |
| `test_singleton` | 3 | 单例一致性、reset、并发访问 |
| `test_pool_config` | 5 | 静态模式、min==max、查询 API、自定义 expiry |
| `test_task_ext` | 10 | Task copy、void 任务、优先级元数据、预创建任务、任务名 |
| **合计** | **56** | 一文一主题，各自独立运行 |

## 线程安全

- 所有 `submit()` 和 `submitSome()` 变体均线程安全
- Worker 管理（创建、回收）内部通过独占锁 + 共享锁同步
- `stealFromRandomWorker` 持共享（读）锁，允许多个 worker 同时窃取；worker 创建/回收持独占锁
- `dumpWorkers()` 可从任意线程调用
- **无死锁回收：** worker 的 shutdown 推迟到池锁释放后执行，防止与并发 stealing 产生锁顺序反转（commit `4457781` 修复）

## 内存安全

- 核心对象（`Pool`、`Worker`、`Task`、`TaskResult`）均通过 `shared_ptr` 管理
- 只需管理 Pool 的生命周期；析构时自动 join 所有 worker 线程
- `asyncDestroyed()` 异步发送 shutdown 信号给所有 worker，返回 `future<bool>`

## 运行时遥测

`Pool::dumpWorkers()` 返回所有 worker 的格式化表格，包含运行时计数器。用于调试负载均衡和 worker 活动。

```cpp
// 任意线程、任意时刻均可调用
std::cout << pool.dumpWorkers() << std::endl;

// 输出示例：
// +-------------------+------+--------+-------+--------+------+------+
// | Thread ID         | Tasks| State  | Steals| Stolen | Idle | Exec |
// +-------------------+------+--------+-------+--------+------+------+
// | 0x7f8c9a0b7640    |    0 | Idle   |    12 |     3  |    5 |  150 |
// | 0x7f8c9a0b7040    |    1 | Busy   |     8 |     5  |    2 |   89 |
// +-------------------+------+--------+-------+--------+------+------+
```

| 列 | 计数器 | 说明 |
|----|--------|------|
| Tasks | `taskCount()` | 该 worker 本地 deque 中待处理的任务数 |
| State | `state()` | `Idle`（空闲）、`Busy`（忙碌）、`Stopped`（已停止） |
| Steals | `stealAttempts_` | 该 worker 尝试从别人那偷任务的次数 |
| Stolen | `tasksStolenAway_` | 该 worker 的任务被其他人偷走的次数 |
| Idle | `idleWaits_` | 该 worker 进入空闲等待（条件变量）的次数 |
| Exec | `tasksExecuted_` | 该 worker 实际执行的任务数 |

以上计数器均为 `std::atomic<uint64_t>`，在 Worker 创建时归零。`dumpWorkers()` 持共享锁遍历 worker 列表，可任意线程安全调用。

## 注意事项

1. 同一个 `Task` 只能提交一次，需要重复执行请使用 `copy()`
2. 一个 `TaskResult` 只能读取一次结果 — 底层 `std::future::get()` 为单次调用
3. Pool 析构会阻塞，直到正在执行的任务完成；使用 `asyncDestroyed()` 实现非阻塞析构
4. `TaskGroup` **不可移动**（`std::counting_semaphore` 不可移动）— 始终使用 `createGroup()`，C++17 保证 NRVO 零开销
5. 优先级当前为元数据，真正的优先级队列排序在计划中
