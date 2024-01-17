file(GLOB_RECURSE headerFiles "*.h")
file(GLOB_RECURSE sourceFiles "*.cpp")
add_library(xworker SHARED ${headerFiles} ${sourceFiles})
set_target_properties(xworker PROPERTIES LINKER_LANGUAGE CXX)
