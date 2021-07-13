add_executable(UnitTest-RngKey rngkey.cpp)
target_link_libraries(UnitTest-RngKey
	test-tools)

add_executable(UnitTest-CmdLineParser clparser.cpp)
target_link_libraries(UnitTest-CmdLineParser
	test-tools clparser)

add_executable(UnitTest-RetroCompatibility compat.cpp)
target_link_libraries(UnitTest-RetroCompatibility
	test-tools xor-runtime clparser)
