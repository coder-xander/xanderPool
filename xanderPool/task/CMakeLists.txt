file(GLOB_RECURSE headerFiles "*.h")
file(GLOB_RECURSE sourceFiles "*.cpp")
add_library(task SHARED ${headerFiles} ${sourceFiles})
set_target_properties(task PROPERTIES LINKER_LANGUAGE CXX)
