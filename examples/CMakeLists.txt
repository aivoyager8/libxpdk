cmake_minimum_required(VERSION 3.15)

# Find the parent library
find_library(XPDK_LIB xpdk PATHS ${CMAKE_BINARY_DIR} NO_DEFAULT_PATH)

# Example programs
add_executable(simple_example simple_example.c)
target_link_libraries(simple_example xpdk ${SPDK_LIBRARIES})

add_executable(async_example async_example.c)
target_link_libraries(async_example xpdk ${SPDK_LIBRARIES})

add_executable(list_devices list_devices.c)
target_link_libraries(list_devices xpdk ${SPDK_LIBRARIES})

add_executable(turbo_example turbo_example.c)
target_link_libraries(turbo_example xpdk ${SPDK_LIBRARIES})

add_executable(turbo_benchmark turbo_benchmark.c)
target_link_libraries(turbo_benchmark xpdk ${SPDK_LIBRARIES})

add_executable(qos_example qos_example.c)
target_link_libraries(qos_example xpdk ${SPDK_LIBRARIES})

add_executable(vectored_example vectored_example.c)
target_link_libraries(vectored_example xpdk ${SPDK_LIBRARIES})

add_executable(advanced_benchmark advanced_benchmark.c)
target_link_libraries(advanced_benchmark xpdk ${SPDK_LIBRARIES})

# Advanced open example (if enabled)
if(ENABLE_ADVANCED_OPEN)
    add_executable(advanced_open_example advanced_open_example.c)
    target_link_libraries(advanced_open_example xpdk ${SPDK_LIBRARIES})
    
    # Install advanced open example
    install(TARGETS advanced_open_example
        RUNTIME DESTINATION bin/examples
    )
endif()

# Install examples
install(TARGETS simple_example async_example list_devices turbo_example turbo_benchmark 
        qos_example vectored_example advanced_benchmark
    RUNTIME DESTINATION bin/examples
)
