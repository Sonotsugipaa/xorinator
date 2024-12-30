add_executable(UnitTest-CmdLineParser clparser.cpp)
target_link_libraries(UnitTest-CmdLineParser
	test-tools clparser)

add_executable(UnitTest-Runtime runtime.cpp)
target_link_libraries(UnitTest-Runtime
	test-tools xor-runtime clparser)
