cmake_minimum_required(VERSION 3.5)

# 设置项目名称和语言
project("xander" LANGUAGES CXX)

# 设置 C++ 标准为 C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 定义一个选项用于控制是否构建测试代码，默认值为 ON
option(XANDERPOOL_BUILD_TEST "Build the tests" ON)


# 添加 src 目录
add_subdirectory(src)

# 如果启用测试，添加 test 目录
if (XANDERPOOL_BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif ()
