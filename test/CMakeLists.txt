# 使用 FetchContent 模块
include(FetchContent)
message("Installing gtest, please wait https://github.com/google/googletest/...")

# 配置 gtest 仓库的拉取
FetchContent_Declare(
        gtest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# 拉取并设置 gtest
FetchContent_MakeAvailable(gtest)

# 查找所有测试源文件
file(GLOB_RECURSE TEST_SOURCES "*.cpp" "*.h")

# find task 测试
add_executable(task_find_test task_find.cc   pool_test.cc)

# 链接库
target_link_libraries(task_find_test PRIVATE xander_pool  gtest gtest_main)

## 注册测试
add_test(NAME task_find_test COMMAND task_find_test)



# 提交测试
add_executable(task_submit_test task_submit_test.cc   pool_test.cc)

# 链接库
target_link_libraries(task_submit_test PRIVATE xander_pool  gtest gtest_main)

## 注册测试
add_test(NAME task_submit_test COMMAND task_submit_test)


#性能测试
add_executable(task_performance_test task_performance.cc   pool_test.cc)
# 链接库
target_link_libraries(task_performance_test PRIVATE xander_pool  gtest gtest_main)

## 注册测试
add_test(NAME task_performance_test COMMAND task_performance_test)


#开发测试
add_executable(dev_test dev_test.cc   pool_test.cc)
# 链接库
target_link_libraries(dev_test PRIVATE xander_pool  gtest gtest_main)

## 注册测试
add_test(NAME dev_test COMMAND task_performance_test)