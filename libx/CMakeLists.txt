

# 子项目xqueue
add_subdirectory(xqueue)
add_subdirectory(xpool)
add_subdirectory(xlock)
add_subdirectory(xtask)
add_subdirectory(xthread)
# 主项目
add_executable (XanderThreadPool "test.cpp" )
# 链接
target_link_libraries(xqueue)
target_link_libraries(xlock)
target_link_libraries(xtask)
target_link_libraries(xthread)
target_link_libraries(xpool)
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET XanderThreadPool PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。