cmake_minimum_required(VERSION 3.15)

project(libxpdk VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Find required packages
find_package(PkgConfig REQUIRED)

# Find SPDK
pkg_check_modules(SPDK REQUIRED spdk_bdev spdk_env_dpdk spdk_thread spdk_util)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${SPDK_INCLUDE_DIRS})

# Library source files
file(GLOB_RECURSE LIBXPDK_SOURCES "src/*.c")

# Create static library
add_library(xpdk STATIC ${LIBXPDK_SOURCES})

# Link libraries
target_link_libraries(xpdk ${SPDK_LIBRARIES})

# Create shared library
add_library(xpdk_shared SHARED ${LIBXPDK_SOURCES})
set_target_properties(xpdk_shared PROPERTIES OUTPUT_NAME xpdk)
target_link_libraries(xpdk_shared ${SPDK_LIBRARIES})

# Install targets
install(TARGETS xpdk xpdk_shared
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY include/ DESTINATION include)

# Examples
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
