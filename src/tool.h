#include <iostream>
#include <chrono>
namespace xander
{
    ///@brief computer the time of finished function you given. and print it to console.
    template <typename F, typename... Args, typename TR = std::conditional_t<std::is_void_v<std::invoke_result_t<F, Args...>>, void, std::invoke_result_t<F, Args...>>>
    TR timeTest(const std::string& functionName, F&& func, Args&&... args)
    {
        using R = std::invoke_result_t<F, Args...>;
        auto start = std::chrono::high_resolution_clock::now();
        if constexpr (!std::is_void_v<R>)
        {
            R result;
            result = std::forward<F>(func)(std::forward<Args>(args)...);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "Function " << functionName << " execution took " << duration.count() << " ms." << std::endl;
            return result;
        }
        else
        {
            std::forward<F>(func)(std::forward<Args>(args)...);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "Function " << functionName << " execution took " << duration.count() << " ms." << std::endl;
        }
    }
#include <random>

    TaskBase::Priority randomPriority()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 3);
        return static_cast<TaskBase::Priority>(dis(gen));
    }
}
