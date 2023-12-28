file(GLOB_RECURSE headerFiles "*.h")
file(GLOB_RECURSE sourceFiles "*.cpp")
add_library(xthread SHARED ${headerFiles} ${sourceFiles})