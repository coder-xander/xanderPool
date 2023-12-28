#include <any>
#include <functional>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <optional>
#include "../xqueue/xqueue.h"
/// @brief 任务的id
class TaskId
{
private:
    std::optional<size_t> id_;

public:
    TaskId() = default;
    TaskId(size_t id) : id_(id) {}
    TaskId(const TaskId &other) : id_(other.id_) {}
    TaskId(TaskId &&other) : id_(std::move(other.id_)) {}

    TaskId &operator=(const TaskId &other)
    {
        id_ = other.id_;
        return *this;
    }

    bool operator==(const TaskId &other) const
    {
        if (!isValid() || !other.isValid())
        {
            return false;
        }
        return id_.value() == other.id_.value();
    }
    bool isValid() const { return id_.has_value(); }
    size_t value() const { return id_.value(); }
    void setValue(size_t id) { id_ = id; }
};

/// @brief 执行结果的返回值
/// @author xander
class ExcuteResult
{
public:
    ExcuteResult(TaskId id, std::any result) : id_(id), result_(result) {}

    TaskId getId() const { return id_; }

    const std::any &toAny() const { return result_; }

    template <typename T>
    T to() const
    {
        if (result_.type() != typeid(T))
        {
            throw std::bad_cast();
        }
        return std::any_cast<T>(result_);
    }
    std::string toString() const
    {
        return to<std::string>();
    }
    int toInt() const
    {
        return to<int>();
    }
    float toFloat() const
    {
        return to<float>();
    }
    double toDouble() const
    {
        return to<double>();
    }
    bool toBool() const
    {
        return to<bool>();
    }

private:
    TaskId id_;
    std::any result_;
};
using ExcuteResultPtr = std::shared_ptr<ExcuteResult>;

/// @brief 任务的基类

class ITask
{
public:
    virtual ~ITask() = default;
    virtual void run() = 0;
    virtual ExcuteResultPtr toAny() = 0;
    virtual TaskId getId() = 0;
};
/// @brief 任务实现类
/// @tparam F 函数
/// @tparam ...Args 参数
/// @tparam R 返回值
using ITaskPtr = std::shared_ptr<ITask>;

template <typename F, typename R, typename... Args>
class Task : public ITask
{
public:
    explicit Task(TaskId id, F &&function, Args &&...args)
        : id_(id), func_(std::bind(std::forward<F>(function), std::forward<Args>(args)...)) {}
    ~Task()
    {
        std::cout << " ~Task" << std::endl;
    }
    void run() override
    {
        result_ = func_();
    }
    TaskId getId() override
    {
        return id_;
    }
    ExcuteResultPtr toAny() override
    {
        return std::make_shared<ExcuteResult>(id_, result_);
    }

private:
    TaskId id_;
    std::function<R()> func_;
    std::any result_;
};
#include <functional>
#include <chrono>
/// @brief 任务管理类
class TaskManager
{
public:
    static std::shared_ptr<TaskManager> makeShared()
    {
        return std::make_shared<TaskManager>();
    }

    template <typename F, typename... Args>
    TaskId add(F &&function, Args &&...args)
    {
        using ReturnType = std::invoke_result_t<F, Args...>;
        auto duration = std::chrono::system_clock::now().time_since_epoch();
        auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        TaskId taskId(std::hash<decltype(now)>()(now));
        while (findTask(taskId) != nullptr)
        {
            duration = std::chrono::system_clock::now().time_since_epoch();
            now = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
            taskId = TaskId(std::hash<decltype(now)>()(now));
        }
        auto task = std::make_shared<Task<F, ReturnType, Args...>>(taskId, std::forward<F>(function), std::forward<Args>(args)...);
        tasks_.enqueue(task);
        return taskId;
    }

    ExcuteResultPtr execute(TaskId taskId)
    {
        auto task = findTask(taskId);
        if (task)
        {
            task->run();
            return task->toAny();
        }
        throw std::runtime_error("Task not found.");
    }
    std::vector<ExcuteResult> executeAll()
    {
        std::vector<ExcuteResult> results;
        auto testTask = tasks_.tryPop();
        while (testTask.has_value())
        {
            {
                testTask.value()->run();
                results.push_back(*testTask.value()->toAny());
            }
            testTask = tasks_.tryPop();
        }
        return results;
    }
    ExcuteResultPtr execute()
    {
        auto task = tasks_.tryPop();
        if (task.has_value())
        {

            task.value()->run();
            return task.value()->toAny();
        }
        else
        {
            throw std::runtime_error("Task not found.");
            return nullptr;
        }
    }
    ITaskPtr findTask(TaskId taskId)
    {
        return tasks_.find([taskId](auto task)
                           { return task->getId() == taskId; });
    }

    bool removeTask(TaskId taskId)
    {
        return tasks_.removeOne([taskId](const ITaskPtr &task)
                                { return task->getId() == taskId.value(); });
    }

    size_t getTaskCount()
    {
        return tasks_.size();
    }

    void clear()
    {
        tasks_.clear();
    }

private:
    XQueue<ITaskPtr> tasks_;
};

using TaskManagerPtr = std::shared_ptr<TaskManager>;
