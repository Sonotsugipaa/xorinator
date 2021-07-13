add_library(clparser STATIC clparser.cpp)
add_library(xor-runtime STATIC runtime.cpp)

# Define a macro with the platform name, for optional runtime UNIX permission checks
if(UNIX)
	target_compile_definitions(xor-runtime PRIVATE XORINATOR_UNIX_PERM_CHECK)
endif()

add_executable(xor main.cpp)
target_link_libraries(xor clparser xor-runtime)

install(TARGETS xor DESTINATION bin)
