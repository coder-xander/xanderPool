#include <iostream>
#include <chrono>

template <typename F, typename... Args, typename TR = std::conditional_t<std::is_void_v<std::invoke_result_t<F, Args...>>, void, std::invoke_result_t<F, Args...>>>
TR timeTest(const std::string& functionName, F&& func, Args&&... args)
{
    // �Ƶ��������ͣ������void�ͷ���void������ʹ�� std::invoke_result_t �жϷ�������
    using R = std::invoke_result_t<F, Args...>;

    // ��¼��ʼʱ��
    auto start = std::chrono::high_resolution_clock::now();

    // ���ú���������÷��ؽ��������еĻ���
    if constexpr (!std::is_void_v<R>)
    {
        R result;
        result = std::forward<F>(func)(std::forward<Args>(args)...);
        // ��¼����ʱ��
        auto end = std::chrono::high_resolution_clock::now();

        // ���㺯��ִ��ʱ�䣬��������
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Function " << functionName << " execution took " << duration.count() << " ms." << std::endl;
        return result;
    }
    else
    {
        std::forward<F>(func)(std::forward<Args>(args)...);
        // ��¼����ʱ��
        auto end = std::chrono::high_resolution_clock::now();

        // ���㺯��ִ��ʱ�䣬��������
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Function " << functionName << " execution took " << duration.count() << " ms." << std::endl;
    }


}
