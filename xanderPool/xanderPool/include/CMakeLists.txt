

# 主项目
add_executable (XanderThreadPool "tool.h" "test.cpp" )
# 链接
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET XanderThreadPool PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。