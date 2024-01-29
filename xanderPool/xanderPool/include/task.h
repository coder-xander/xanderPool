#pragma once
#include <memory>
#include <functional>
#include <any>
#include <future>
#include "taskResult.h"

namespace xander
{
	/// @brief the task base class
	class TaskBase :public  std::enable_shared_from_this<TaskBase>
	{

	public:
		enum Priority
		{
			High,
			Normal,
			low
		};
	protected:
		//normal level is default priority
		Priority 	priority_;
	public:
		void setPriority(const Priority& priority)
		{
			priority_ = priority;
		}
		const auto& priority()const
		{
			return priority_;
		}
		auto taskResult() {};
		virtual ~TaskBase() = default;
		virtual std::shared_ptr<TaskBase> run() = 0;
	};

	/// @brief the task class 
	/// @tparam F function type
	/// @tparam ...Args arguments type
	/// @tparam R return value type
	template <typename F, typename R, typename... Args >
	class Task final : public TaskBase
	{

	public:
		///@brief constructor,will bind function with args to a std::packaged_task,and the default priority of task is Normal.
		explicit Task(F&& function, Args&&... args)
			: packagedFunc_(std::bind(std::forward<F>(function), std::forward<Args>(args)...))
		{
			priority_ = Priority::Normal;
			taskResultPtr_ = TaskResult<R>::makeShared(std::move(getTaskPackaged().get_future()));

		}
		explicit Task(std::packaged_task<R(Args ...)> packagedTask) :packagedFunc_(std::move(packagedTask))
		{
			priority_ = Priority::Normal;
			taskResultPtr_ = TaskResult<R>::makeShared(std::move(getTaskPackaged().get_future()));

		}
		Task() = delete;
		///@brief destructor,automatic
		~Task() override
		{
			// std::cout << " ~Task" << std::endl;
		}
		///@brief run the task,when run done,future will be gettable.
		std::shared_ptr<TaskBase>  run() override
		{
			packagedFunc_();
			return shared_from_this();
		}
		///@brief this function will be called by pool.give a taskResult to decorate task`s result.
		void setTaskResult(TaskResultPtr<R> taskResultPtr)
		{
			taskResultPtr_ = taskResultPtr;
		}
		void build()
		{
			taskResultPtr_->setTask(shared_from_this());
		}
		///@brief get TaskResultPtr
		auto taskResult()
		{
			return taskResultPtr_;
		}
		///@brief  get the packagedFunc_,
		std::packaged_task<R()>& getTaskPackaged()
		{
			return packagedFunc_;
		}

	private:
		//blinded function
		std::packaged_task<R() > packagedFunc_;
		//task result decorated by TaskResultPtr
		TaskResultPtr<R> taskResultPtr_;

	};
	using TaskBasePtr = std::shared_ptr<TaskBase>;
	///@brief make a task,please use this function to create  a task .
	template <typename F, typename... Args, typename R = std::invoke_result_t<F, Args...>>
	std::shared_ptr<Task<F, R, Args...>> makeTask(F&& function, Args&&... args, TaskBase::Priority priority = TaskBase::Normal)
	{
		auto task = std::make_shared<Task<F, R, Args...>>(std::forward<F>(function), std::forward<Args>(args) ...);
		task->setPriority(priority);
		task->build();
		return task;
	}
}
