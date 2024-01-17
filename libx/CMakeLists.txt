

# 子项目xqueue
add_subdirectory(queue)
add_subdirectory(pool)
add_subdirectory(semaphoreGuard)
add_subdirectory(task)
add_subdirectory(worker)
# 主项目
add_executable (XanderThreadPool "test.cpp" )
# 链接
target_link_libraries(queue)
target_link_libraries(SemaphoreGuard)
target_link_libraries(task)
target_link_libraries(pool)
target_link_libraries(worker)
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET XanderThreadPool PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。