add_library(clparser STATIC
	clparser.cpp)

add_executable(xor main.cpp)
target_link_libraries(xor clparser)

install(TARGETS xor DESTINATION bin)
