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

# ============================================================
# 每个测试一个独立的可执行文件，一个文件测试一个主题
# ============================================================

# --- 查找任务 ---
add_executable(task_find_test task_find.cc   pool_test.cc)
target_link_libraries(task_find_test PRIVATE xander_pool  gtest gtest_main)
add_test(NAME task_find_test COMMAND task_find_test)

# --- 基础提交 ---
add_executable(task_submit_test task_submit_test.cc   pool_test.cc)
target_link_libraries(task_submit_test PRIVATE xander_pool  gtest gtest_main)
add_test(NAME task_submit_test COMMAND task_submit_test)

# --- 性能 ---
add_executable(task_performance_test task_performance.cc   pool_test.cc)
target_link_libraries(task_performance_test PRIVATE xander_pool  gtest gtest_main)
add_test(NAME task_performance_test COMMAND task_performance_test)

# --- 开发（快速创建销毁）---
add_executable(dev_test dev_test.cc   pool_test.cc)
target_link_libraries(dev_test PRIVATE xander_pool  gtest gtest_main)
add_test(NAME dev_test COMMAND dev_test)

# --- Work-Stealing ---
add_executable(test_work_stealing test_work_stealing.cc pool_test.cc)
target_link_libraries(test_work_stealing PRIVATE xander_pool gtest gtest_main)
add_test(NAME test_work_stealing COMMAND test_work_stealing)

# --- TaskGroup ---
add_executable(test_task_group test_task_group.cc pool_test.cc)
target_link_libraries(test_task_group PRIVATE xander_pool gtest gtest_main)
add_test(NAME test_task_group COMMAND test_task_group)

# --- 优先级 ---
add_executable(test_priority test_priority.cc pool_test.cc)
target_link_libraries(test_priority PRIVATE xander_pool gtest gtest_main)
add_test(NAME test_priority COMMAND test_priority)

# --- 并发安全 ---
add_executable(test_concurrent_safety test_concurrent_safety.cc pool_test.cc)
target_link_libraries(test_concurrent_safety PRIVATE xander_pool gtest gtest_main)
add_test(NAME test_concurrent_safety COMMAND test_concurrent_safety)

# --- 批量提交 ---
add_executable(test_batch test_batch.cc pool_test.cc)
target_link_libraries(test_batch PRIVATE xander_pool gtest gtest_main)
add_test(NAME test_batch COMMAND test_batch)

# --- 超时 ---
add_executable(test_timeout test_timeout.cc pool_test.cc)
target_link_libraries(test_timeout PRIVATE xander_pool gtest gtest_main)
add_test(NAME test_timeout COMMAND test_timeout)

# --- 动态扩缩 ---
add_executable(test_scaling test_scaling.cc pool_test.cc)
target_link_libraries(test_scaling PRIVATE xander_pool gtest gtest_main)
add_test(NAME test_scaling COMMAND test_scaling)

# --- Worker 回收（死锁回归）---
add_executable(test_reclaim test_reclaim.cc pool_test.cc)
target_link_libraries(test_reclaim PRIVATE xander_pool gtest gtest_main)
add_test(NAME test_reclaim COMMAND test_reclaim)

# --- WorkStealingDeque 全面 ---
add_executable(test_deque test_deque.cc)
target_link_libraries(test_deque PRIVATE gtest gtest_main)
target_include_directories(test_deque PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_deque COMMAND test_deque)

# --- 单例模式 ---
add_executable(test_singleton test_singleton.cc pool_test.cc)
target_link_libraries(test_singleton PRIVATE xander_pool gtest gtest_main)
add_test(NAME test_singleton COMMAND test_singleton)

# --- Pool 配置与查询 ---
add_executable(test_pool_config test_pool_config.cc pool_test.cc)
target_link_libraries(test_pool_config PRIVATE xander_pool gtest gtest_main)
add_test(NAME test_pool_config COMMAND test_pool_config)

# --- Task 扩展 ---
add_executable(test_task_ext test_task_ext.cc pool_test.cc)
target_link_libraries(test_task_ext PRIVATE xander_pool gtest gtest_main)
add_test(NAME test_task_ext COMMAND test_task_ext)
