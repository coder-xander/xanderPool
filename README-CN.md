# xanderPool

[English](README.md)

![GitHub last commit](https://img.shields.io/github/last-commit/coder-xander/xanderPool)
![Language](https://img.shields.io/badge/language-C%2B%2B20-blue)
![License](https://img.shields.io/github/license/coder-xander/xanderPool)
![Header Only](https://img.shields.io/badge/header--only-yes-green)

高性能、跨平台、header-only 的 C++20 线程池，支持任务优先级。纯标准库实现，零外部依赖，拷贝即用。

## 特性

- **Header-only** — 拷贝 `src/` 到项目中即可，无需构建系统
- **任务优先级** — 高/普通/低，始终优先执行最高优先级
- **动态扩缩容** — 高负载自动创建 worker，空闲自动回收
- **静态模式** — 固定 worker 数量，适合确定性场景
- **内存安全** — 全程 `shared_ptr`，只需管理 Pool 生命周期
- **线程安全** — 所有 `submit` / `submitSome` 并发安全
- **多种可调用对象** — lambda、全局函数、成员函数、仿函数
- **异步结果** — `TaskResult` 支持阻塞等待和超时等待
- **零依赖** — 纯 C++ 标准库

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
  - [任务优先级](#任务优先级)
  - [获取结果](#获取结果)
  - [批量提交](#批量提交)
  - [任务复制](#任务复制)
  - [直接使用 Worker](#直接使用-worker)
- [构建与测试](#构建与测试)
- [性能](#性能)
- [线程安全](#线程安全)
- [内存安全](#内存安全)
- [注意事项](#注意事项)
- [联系方式](#联系方式)

## 架构

```
┌─────────────────────────────────────────────────┐
│                   Pool（管理器）                 │
│  ┌──────────────────────────────────────────┐   │
│  │  任务分配策略                             │   │
│  │  1. 有空闲 worker → 直接分配             │   │
│  │  2. 全忙 + 未达上限 → 创建新 worker      │   │
│  │  3. 全忙 + 已达上限 → 分给任务最少的     │   │
│  └──────────────────────────────────────────┘   │
│                                                 │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐           │
│  │Worker 1 │ │Worker 2 │ │Worker N │  (自动    │
│  │ ┌─────┐ │ │ ┌─────┐ │ │ ┌─────┐ │   扩缩)   │
│  │ │High │ │ │ │High │ │ │ │High │ │           │
│  │ │Norm │ │ │ │Norm │ │ │ │Norm │ │           │
│  │ │Low  │ │ │ │Low  │ │ │ │Low  │ │           │
│  │ └─────┘ │ │ └─────┘ │ │ └─────┘ │           │
│  └─────────┘ └─────────┘ └─────────┘           │
│                                                 │
│  GC 线程 ── 定期回收空闲 worker                  │
└─────────────────────────────────────────────────┘

submit() → TaskResult<T> → syncGetResult() / syncGetResult(ms)
```

## API 参考

### 创建 Pool

```cpp
// 单例模式（全局推荐）
Pool* pool = Pool::instance();

// 局部实例
Pool pool;                        // 动态，min=2, max=CPU核心数
Pool pool(4, 8);                  // 动态，min=4, max=8
Pool pool(4, 8, 3000);            // + 自定义 GC 间隔（毫秒）

// 静态模式（固定 worker 数量）
pool.useStaticMode();             // worker = CPU核心数
pool.useStaticMode(4);            // worker = 4
```

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
auto task = makeTask(TaskBase::Normal, []() { return 1; });
pool.submit(task);
```

### 任务优先级

| 优先级 | 枚举值 | 行为 |
|--------|--------|------|
| 高 | `TaskBase::High` | 最先执行 |
| 普通 | `TaskBase::Normal` | 默认 |
| 低 | `TaskBase::low` | 最后执行 |

每个 worker 维护三个独立队列，始终从最高优先级的非空队列取任务。

### 获取结果

```cpp
auto result = pool.submit([]() { return 42; });

// 阻塞等待
int value = result->syncGetResult();

// 带超时（返回 std::optional）
auto opt = result->syncGetResult(100); // 100ms 超时
if (opt.has_value()) {
    int v = opt.value();
}
```

### 批量提交

```cpp
auto t1 = makeTask([]() { return 1; });
auto t2 = makeTask([]() { return "hello"; });

pool.submitSome({t1, t2});

// 通过 task 对象获取结果
int r1 = t1->getTaskResult()->syncGetResult();
```

`submitSome` 无返回值（因为任务类型可能不同），通过 task 对象获取各自的 TaskResult。

### 任务复制

同一个 Task 只能提交一次，再次提交需要 `copy()`：

```cpp
pool.submit(task1);
pool.submit(task1->copy());                                // 再次执行
pool.submit(task2->copy()->setPriority(TaskBase::High));   // 复制 + 提升优先级
```

### 直接使用 Worker

绕过 Pool，使用单线程任务队列：

```cpp
WorkerPtr worker = Worker::makeShared();
auto result = worker->submit([]() {
    return 1 + 2;
});
result->syncGetResult(200);
```

适用于需要多个任务在同一线程中顺序执行的场景。

## 构建与测试

### Header-Only 用法

将 `src/` 目录拷贝到项目中：

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
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
ctest --timeout 30
```

测试依赖 [Google Test](https://github.com/google/googletest)（CMake 自动拉取）。

### 测试覆盖

| 测试套件 | 说明 |
|---------|------|
| `task_submit_test` | 全局函数、成员函数、lambda、仿函数提交 |
| `task_find_test` | 按名称在 worker 中查找任务 |
| `task_performance_test` | 1000 个任务，验证全部完成 |
| `dev_test` | 1000 次 Pool 创建/销毁循环 |
| `comprehensive_test` | 优先级排序、并发提交、超时、批量提交、动态扩缩容 |

## 性能

10代 i5，Release 构建，提交 100,000 个空任务：

```
~135ms 完成入队
Worker 自动扩缩应对突发负载
```

动态扩缩容 + GC 回收机制确保不同负载下的资源高效利用。

## 线程安全

- 所有 `submit()` 和 `submitSome()` 变体均线程安全
- Worker 管理（创建、GC 回收）内部同步
- `dumpWorkers()` 可从任意线程调用

## 内存安全

- 核心对象（`Pool`、`Worker`、`Task`、`TaskResult`）均通过 `shared_ptr` 管理
- 只需管理 `Pool` 的生命周期
- Pool 析构时等待正在执行的任务完成
- `asyncDestroyed()` 支持非阻塞析构

## 注意事项

1. 同一个 `Task` 只能提交一次，需要重复执行请使用 `copy()`
2. 一个 `TaskResult` 只能读取一次结果，重复调用 `syncGetResult` 为未定义行为
3. Pool 析构会阻塞，直到正在执行的任务完成

## 联系方式

邮箱：xhr1028@foxmail.com
