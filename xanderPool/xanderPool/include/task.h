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
        Priority 	priority_ = Normal;
    public:
        void setPriority(const Priority& priority)
        {
            priority_ = priority;
        }
        const auto& priority()const
        {
            return priority_;
        }
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
        ///@brief  copy a task already exist.callable function and priority will be copied.it will take few time.
        std::shared_ptr<Task<F, R, Args ...>>copy()
        {
            return std::make_shared<Task<F, R, Args ...>>(f_, priority_);

        }
        ///@brief constructor,will bind function with args to a std::packaged_task,and the default priority of task is Normal.
        explicit Task(F&& function, Args&&... args) :packagedFunc_(std::bind(std::forward<F>(function), std::forward<Args>(args)...))
        {
            f_ = std::bind(std::forward<F>(function), std::forward<Args>(args)...);
            priority_ = Priority::Normal;
            taskResultPtr_ = TaskResult<R>::makeShared(std::move(getTaskPackaged().get_future()));

        }
        explicit Task(std::function<R()> f, const Priority& priority) :packagedFunc_(f)
        {

            f_ = f;
            priority_ = priority;
            taskResultPtr_ = TaskResult<R>::makeShared(std::move(getTaskPackaged().get_future()));

        }
    private:
        Task() = default;
    public:
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
        ///@brief this function will be called by pool.give a getTaskResult to decorate task`s result.
        void setTaskResult(TaskResultPtr<R> taskResultPtr)
        {
            taskResultPtr_ = taskResultPtr;
        }
        ///@brief why so simple a function ? the constructor is not good to obtain "this".
        void build()
        {
            taskResultPtr_->setTask(shared_from_this());
        }
        ///@brief sync get result of task,will wait until result is ready,if result is void,return void,remember you can only call once.you also could call getTaskResult() first ,and get the taskResult with it
        R syncGetResult()
        {
            return taskResultPtr_->syncGetResult();
        }
        /// @brief sync get result of task in timeout,if result is void,return void,remember you can only call once.
        std::conditional_t<std::is_same_v<void, R>, void, std::optional<R>> syncGetResult(int timeout)
        {
            return taskResultPtr_->syncGetResult(timeout);
        }
        ///@brief get TaskResultPtr
        TaskResultPtr<R> getTaskResult()
        {
            return taskResultPtr_;
        }
        ///@brief  get the packagedFunc_,
        std::packaged_task<R()>& getTaskPackaged()
        {
            return packagedFunc_;
        }

    private:
        //recorded for copy
        std::function<R()> f_;
        //blinded function
        std::packaged_task<R() > packagedFunc_;
        //task result decorated by TaskResultPtr
        TaskResultPtr<R> taskResultPtr_;

    };
    using TaskBasePtr = std::shared_ptr<TaskBase>;
    template <typename F, typename R, typename... Args>
    using TaskPtr = std::shared_ptr<Task<F, R, Args...>>;
    ///@brief make a task,please use this function to create  a task .
    template <typename F, typename... Args, typename R = typename std::invoke_result_t<F, Args...>>
    TaskPtr<F, R, Args...> makeTask(F&& function, Args&&... args)
    {
        auto task = std::make_shared<Task<F, R, Args...>>(std::forward<F>(function), std::forward<Args>(args) ...);
        task->setPriority(TaskBase::Normal);
        task->build();
        return task;
    }
    ///@brief make a task,please use this function to create  a task .
    template <typename F, typename... Args, typename R = typename std::invoke_result_t<F, Args...>>
    std::shared_ptr<Task<F, R, Args...>> makeTask(const TaskBase::Priority& priority, F&& function, Args&&... args)
    {
        auto task = std::make_shared<Task<F, R, Args...>>(std::forward<F>(function), std::forward<Args>(args) ...);
        task->setPriority(priority);
        task->build();
        return task;
    }
}
