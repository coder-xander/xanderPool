

# 子项目xqueue
add_subdirectory(queue)
add_subdirectory(pool)
add_subdirectory(worker)
add_subdirectory(task)
# 主项目
add_executable (XanderThreadPool "tool.h" "test.cpp" )
# 链接
target_link_libraries(queue)
target_link_libraries(task)
target_link_libraries(worker)
target_link_libraries(pool)
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET XanderThreadPool PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。