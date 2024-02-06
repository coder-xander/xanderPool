
set(CMAKE_CXX_STANDARD 17)
# 确保设置是正确传递给编译器的
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 主项目
add_executable (XanderThreadPool "tool.h" "test.cpp" )
# 链接
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET XanderThreadPool PROPERTY CXX_STANDARD 20)
endif()
