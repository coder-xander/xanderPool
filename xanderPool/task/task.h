#pragma once
#include <memory>
#include <functional>
#include <any>
#include <future>
#include "taskResult.h"

namespace xander
{
	/// @brief 任务的基类
	class TaskBase :public  std::enable_shared_from_this<TaskBase>
	{
	public:
		virtual ~TaskBase() = default;
		virtual std::shared_ptr<TaskBase> run() = 0;
		virtual const std::string& getId() = 0;
	};

	/// @brief 任务实现类
	/// @tparam F 函数
	/// @tparam ...Args 参数
	/// @tparam R 返回值
	template <typename F, typename R, typename... Args >
	class Task final : public TaskBase
	{
	public:
		//要么是void要么就是any
		explicit Task(const std::string& id, F&& function, Args &&...args)
			: id_(id), packagedFunc_(std::bind(std::forward<F>(function), std::forward<Args>(args)...))
		{
		}
		~Task() override
		{
			// std::cout << " ~Task" << std::endl;
		}
		std::shared_ptr<TaskBase>  run() override
		{

			packagedFunc_();
			return shared_from_this();
		}
		const std::string& getId() override
		{
			return id_;
		}
		void setTaskResult(TaskResultPtr<R> taskResultPtr)
		{

			taskResultPtr_ = taskResultPtr;
		}
		std::packaged_task<R()>& getTaskPackaged()
		{
			return packagedFunc_;
		}

	private:
		std::string  id_;
		std::packaged_task<R() > packagedFunc_;
		TaskResultPtr<R> taskResultPtr_;
	};
	using TaskBasePtr = std::shared_ptr<TaskBase>;
}
