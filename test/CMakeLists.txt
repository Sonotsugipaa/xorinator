include_directories(..)
include_directories(.)

# Add component source directory
add_subdirectory(tests)


# Process top-level targets

	function(copy_file file)
		configure_file(
			${CMAKE_CURRENT_SOURCE_DIR}/${file}
			${CMAKE_CURRENT_BINARY_DIR}/${file}
			COPYONLY)
	endfunction()

	copy_file(run-tests.sh)

	add_library(test-tools STATIC
		test_tools.cpp)
