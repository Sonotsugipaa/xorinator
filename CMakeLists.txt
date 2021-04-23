# Declare the project

	cmake_minimum_required(VERSION 3.19)
	project(xorinator)


# Set the project's structure

	set(PROJECT_SOURCE_DIR .)
	set(PROJECT_BINARY_DIR .)

	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

	include_directories(include)
	link_directories(lib)
	link_directories(bin)


# Set warning levels

	if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
		add_compile_options(-Wall -Wextra -Wpedantic)
	elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
		add_compile_options(\Wall)
	else()
		message(WARNING "Toolchain \"${CMAKE_CXX_COMPILER_ID}\" not recognized by the project")
	endif()


# Define the C and C++ standards to use

	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED True)


# Add the main and test subdirectories

	add_subdirectory(${PROJECT_SOURCE_DIR}/cli-tool)
	add_subdirectory(${PROJECT_SOURCE_DIR}/test)
