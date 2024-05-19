#遍历当前目录下的所有源文件
file(GLOB_RECURSE DIR_LIB_SRCS
        "*.cpp"
        "*.h"
)
# 创建库目标
add_library(xander_pool STATIC ${DIR_LIB_SRCS})
set_target_properties(xander_pool PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(xander_pool PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
