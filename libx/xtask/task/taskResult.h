#pragma once
#include <any>
#include <functional>
#include "taskid.h"
#include <iostream>
/// @brief 执行结果的返回值
/// @author xander
class ExcuteResult
{
public:
    ExcuteResult(TaskIdPtr id, std::any result) : id_(id), result_(result) {}
    ExcuteResult(TaskId id, std::any result) : id_(std::make_shared<TaskId>(id)), result_(result) {}
    auto getId() const { return id_; }
    ~ExcuteResult() { std::cout << " ~ExcuteResult" << std::endl; }
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
    TaskIdPtr id_;
    std::any result_;
};
using ExcuteResultPtr = std::shared_ptr<ExcuteResult>;
