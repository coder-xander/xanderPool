# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
cmake_minimum_required (VERSION 3.8 )
set(CMAKE_CXX_STANDARD 17)
# 确保设置是正确传递给编译器的
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project ("xanderPool")
# 包含子项目。
add_subdirectory ("XanderPool")


