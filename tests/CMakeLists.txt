cmake_minimum_required(VERSION 3.15)

# Find required packages for testing
find_package(PkgConfig REQUIRED)

# Simple test framework
add_executable(test_basic test_basic.c)
target_link_libraries(test_basic xpdk ${SPDK_LIBRARIES})

add_executable(test_io test_io.c)
target_link_libraries(test_io xpdk ${SPDK_LIBRARIES})

add_executable(test_async test_async.c)
target_link_libraries(test_async xpdk ${SPDK_LIBRARIES})

add_executable(test_qos test_qos.c)
target_link_libraries(test_qos xpdk ${SPDK_LIBRARIES})

add_executable(test_batch test_batch.c)
target_link_libraries(test_batch xpdk ${SPDK_LIBRARIES})

add_executable(test_integration test_integration.c)
target_link_libraries(test_integration xpdk ${SPDK_LIBRARIES})

# Add tests
add_test(NAME basic_functionality COMMAND test_basic)
add_test(NAME io_operations COMMAND test_io)
add_test(NAME async_operations COMMAND test_async)
add_test(NAME qos_functionality COMMAND test_qos)
add_test(NAME batch_operations COMMAND test_batch)
add_test(NAME integration_test COMMAND test_integration)

# Install tests
install(TARGETS test_basic test_io test_async test_qos test_batch test_integration
    RUNTIME DESTINATION bin/tests
)
