#include <iostream>
#include <chrono>

template <typename F, typename... Args, typename TR = std::conditional_t<std::is_void_v<std::invoke_result_t<F, Args...>>, void, std::invoke_result_t<F, Args...>>>
TR timeTest(const std::string& functionName, F&& func, Args&&... args)
{
    // 推导返回类型，如果是void就返回void，否则使用 std::invoke_result_t 判断返回类型
    using R = std::invoke_result_t<F, Args...>;

    // 记录开始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 调用函数，并获得返回结果（如果有的话）
    if constexpr (!std::is_void_v<R>)
    {
        R result;
        result = std::forward<F>(func)(std::forward<Args>(args)...);
        // 记录结束时间
        auto end = std::chrono::high_resolution_clock::now();

        // 计算函数执行时间，并输出结果
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Function " << functionName << " execution took " << duration.count() << " ms." << std::endl;
        return result;
    }
    else
    {
        std::forward<F>(func)(std::forward<Args>(args)...);
        // 记录结束时间
        auto end = std::chrono::high_resolution_clock::now();

        // 计算函数执行时间，并输出结果
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Function " << functionName << " execution took " << duration.count() << " ms." << std::endl;
    }


}
