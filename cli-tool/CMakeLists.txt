add_library(clparser STATIC clparser.cpp)
add_library(xor-runtime STATIC runtime.cpp)

add_executable(xor main.cpp)
target_link_libraries(xor clparser xor-runtime)

install(TARGETS xor DESTINATION bin)
