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

# Find SPDK - use core libraries with proper dependency order
pkg_check_modules(SPDK REQUIRED 
    spdk_bdev 
    spdk_env_dpdk 
    spdk_thread 
    spdk_util
)

# Find additional required libraries for SPDK
find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED uuid)
find_library(SSL_LIB ssl)
find_library(CRYPTO_LIB crypto)
find_library(ISAL_LIB isal)
find_library(ISAL_CRYPTO_LIB isal_crypto)
find_library(NUMA_LIB numa)
find_library(DL_LIB dl)
find_library(PTHREAD_LIB pthread)

# Combine all libraries with proper order for static linking
# Use --start-group/--end-group to resolve circular dependencies in static libraries
set(ALL_LIBRARIES -Wl,--start-group ${SPDK_LIBRARIES} ${UUID_LIBRARIES} -Wl,--end-group)
if(SSL_LIB)
    list(APPEND ALL_LIBRARIES ${SSL_LIB})
endif()
if(CRYPTO_LIB)
    list(APPEND ALL_LIBRARIES ${CRYPTO_LIB})
endif()
if(ISAL_LIB)
    list(APPEND ALL_LIBRARIES ${ISAL_LIB})
endif()
if(ISAL_CRYPTO_LIB)
    list(APPEND ALL_LIBRARIES ${ISAL_CRYPTO_LIB})
endif()
if(NUMA_LIB)
    list(APPEND ALL_LIBRARIES ${NUMA_LIB})
endif()
if(DL_LIB)
    list(APPEND ALL_LIBRARIES ${DL_LIB})
endif()
if(PTHREAD_LIB)
    list(APPEND ALL_LIBRARIES ${PTHREAD_LIB})
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${SPDK_INCLUDE_DIRS})
include_directories(${UUID_INCLUDE_DIRS})

# 自动修复：补充 SPDK 头文件和库路径，确保 spdk/app.h 可见
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/spdk/include/spdk/app.h")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/spdk/include)
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/spdk/build/lib)
endif()

# Library source files - explicitly exclude SPDK subdirectory and non-native vectored implementation
file(GLOB LIBXPDK_SOURCES 
    "src/xpdk_*.c"
)

# Remove any SPDK subdirectory files and standard vectored implementation
list(FILTER LIBXPDK_SOURCES EXCLUDE REGEX "src/spdk/.*")
list(FILTER LIBXPDK_SOURCES EXCLUDE REGEX "src/xpdk_vectored.c")

# Create static library
add_library(xpdk STATIC ${LIBXPDK_SOURCES})

# Link libraries
target_link_libraries(xpdk ${ALL_LIBRARIES})

# Create shared library
add_library(xpdk_shared SHARED ${LIBXPDK_SOURCES})
set_target_properties(xpdk_shared PROPERTIES OUTPUT_NAME xpdk)
target_link_libraries(xpdk_shared ${ALL_LIBRARIES})

# Install targets
install(TARGETS xpdk xpdk_shared
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY include/ DESTINATION include)

# Advanced open functionality
option(ENABLE_ADVANCED_OPEN "Enable advanced open mechanism" ON)
if(ENABLE_ADVANCED_OPEN)
    target_compile_definitions(xpdk PRIVATE XPDK_ENABLE_ADVANCED_OPEN)
    target_compile_definitions(xpdk_shared PRIVATE XPDK_ENABLE_ADVANCED_OPEN)
endif()

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
