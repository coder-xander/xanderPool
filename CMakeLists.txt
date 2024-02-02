# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
cmake_minimum_required (VERSION 3.8 )
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON) #设置版本要求
set(CMAKE_CXX_EXTENSIONS OFF) #关闭编译器扩展
project ("xanderPool")
# 包含子项目。
add_subdirectory ("XanderPool")


