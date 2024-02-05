cmake_minimum_required(VERSION 3.20)

project(sgc)

set(BUILD_DIRECTORY_NAME OUTPUT)

# Add executable targets.
message(STATUS "Adding executable targets...")

# Add project tests target.
set(PROJECT_TESTS_DIRECTORY sgc_tests)
message(STATUS "Adding target ${PROJECT_TESTS_DIRECTORY}...")
add_subdirectory(src/${PROJECT_TESTS_DIRECTORY} ${BUILD_DIRECTORY_NAME}/${PROJECT_TESTS_DIRECTORY})
