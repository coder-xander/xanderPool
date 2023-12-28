#pragma once
#include <optional>
#include <memory>
#include <iostream>
/// @brief 任务的id
class TaskId
{
private:
    std::optional<size_t> id_;

public:
    static std::shared_ptr<TaskId> makeShared()
    {
        return std::make_shared<TaskId>();
    }
    TaskId() = default;
    TaskId(size_t id) : id_(id) {}
    TaskId(const TaskId &other) : id_(other.id_) {}
    TaskId(TaskId &&other) : id_(std::move(other.id_)) {}
    ~TaskId()
    {
        std::cout << "TaskId::~TaskId()" << std::endl;
    }
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
using TaskIdPtr = std::shared_ptr<TaskId>;