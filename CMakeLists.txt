cmake_minimum_required(VERSION 3.20)

project(sgc)

# Configurable options:
option(SGC_ENABLE_TESTS "Defines whether to add tests target or not." ON)

# Define name of the output directory.
set(BUILD_DIRECTORY_NAME OUTPUT)

# Add sgc lib.
set(SGC_LIB_NAME sgc_lib)
message(STATUS "Adding target ${SGC_LIB_NAME}...")
add_subdirectory(src/${SGC_LIB_NAME} ${BUILD_DIRECTORY_NAME}/${SGC_LIB_NAME})

if (SGC_ENABLE_TESTS)
    # Add sgc tests.
    set(SGC_TESTS_NAME sgc_tests)
    message(STATUS "Adding target ${SGC_TESTS_NAME}...")
    add_subdirectory(src/${SGC_TESTS_NAME} ${BUILD_DIRECTORY_NAME}/${SGC_TESTS_NAME})
endif()
