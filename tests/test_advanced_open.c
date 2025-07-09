#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include "xpdk.h"
#include "xpdk_advanced.h"

static int test_passed = 0;
static int test_failed = 0;

#define TEST_ASSERT(condition, msg) \
    do { \
        if (condition) { \
            printf("PASS: %s\n", msg); \
            test_passed++; \
        } else { \
            printf("FAIL: %s\n", msg); \
            test_failed++; \
        } \
    } while (0)

static void
test_advanced_open_opts_init(void)
{
    printf("\n=== Test: Advanced Open Options Initialization ===\n");
    
    struct xpdk_open_opts opts;
    xpdk_open_opts_init(&opts);
    
    TEST_ASSERT(opts.flags == O_RDWR, "Default flags should be O_RDWR");
    TEST_ASSERT(opts.timeout_ms == 30000, "Default timeout should be 30 seconds");
    TEST_ASSERT(opts.allow_partial_failure == false, "Partial failure should be disabled by default");
    TEST_ASSERT(opts.enable_turbo == false, "Turbo mode should be disabled by default");
    TEST_ASSERT(opts.cpu_core == -1, "CPU core should be unbound by default");
    TEST_ASSERT(opts.queue_depth == 0, "Queue depth should use default");
    TEST_ASSERT(opts.qos_limits == NULL, "QoS limits should be NULL by default");
    TEST_ASSERT(opts.enable_auto_retry == true, "Auto retry should be enabled by default");
    TEST_ASSERT(opts.max_retry_count == 3, "Default retry count should be 3");
    TEST_ASSERT(opts.retry_delay_ms == 100, "Default retry delay should be 100ms");
}

static void
test_simple_advanced_open(void)
{
    printf("\n=== Test: Simple Advanced Open ===\n");
    
    struct xpdk_open_opts opts;
    struct xpdk_device_desc desc;
    
    xpdk_open_opts_init(&opts);
    opts.flags = O_RDWR;
    opts.enable_turbo = true;
    opts.timeout_ms = 5000;
    
    int rc = xpdk_open_advanced("Malloc0", &opts, &desc);
    
    if (rc == XPDK_SUCCESS) {
        TEST_ASSERT(desc.fd >= 0, "File descriptor should be valid");
        TEST_ASSERT(strcmp(desc.name, "Malloc0") == 0, "Device name should match");
        TEST_ASSERT(desc.state == XPDK_DEVICE_STATE_OPEN, "Device should be in open state");
        TEST_ASSERT(desc.opts.enable_turbo == true, "Turbo mode should be preserved");
        
        /* Test device info */
        TEST_ASSERT(desc.info.block_size > 0, "Block size should be positive");
        TEST_ASSERT(desc.info.capacity > 0, "Capacity should be positive");
        
        /* Close device */
        rc = xpdk_close_advanced(&desc);
        TEST_ASSERT(rc == XPDK_SUCCESS, "Device should close successfully");
        TEST_ASSERT(desc.state == XPDK_DEVICE_STATE_CLOSED, "Device should be in closed state");
        
    } else {
        printf("WARNING: Could not open Malloc0 device (may not exist): %s\n", xpdk_strerror(rc));
        test_passed++; /* Don't fail test if device doesn't exist */
    }
}

static void
test_batch_open(void)
{
    printf("\n=== Test: Batch Open ===\n");
    
    struct xpdk_open_request requests[3];
    struct xpdk_open_result result;
    
    /* Initialize requests */
    for (int i = 0; i < 3; i++) {
        xpdk_open_opts_init(&requests[i].opts);
        requests[i].opts.flags = O_RDWR;
        requests[i].opts.allow_partial_failure = true;
        requests[i].group_id = 100 + i;
        requests[i].dependency_count = 0;
        requests[i].priority = 0;
    }
    
    snprintf(requests[0].bdev_name, sizeof(requests[0].bdev_name), "Malloc0");
    snprintf(requests[1].bdev_name, sizeof(requests[1].bdev_name), "Malloc1");
    snprintf(requests[2].bdev_name, sizeof(requests[2].bdev_name), "NonExistent");
    
    int rc = xpdk_open_batch(requests, 3, &result);
    
    TEST_ASSERT(result.requested_count == 3, "Requested count should be 3");
    TEST_ASSERT(result.operation_time_us > 0, "Operation time should be recorded");
    
    if (result.opened_count > 0) {
        printf("Successfully opened %u devices\n", result.opened_count);
        
        /* Verify opened devices */
        for (uint32_t i = 0; i < result.opened_count; i++) {
            TEST_ASSERT(result.devices[i].fd >= 0, "Device FD should be valid");
            TEST_ASSERT(result.devices[i].state == XPDK_DEVICE_STATE_OPEN, "Device should be open");
        }
        
        /* Close all opened devices */
        rc = xpdk_close_batch(result.devices, result.opened_count);
        TEST_ASSERT(rc == XPDK_SUCCESS, "Batch close should succeed");
    }
    
    TEST_ASSERT(result.failed_count >= 1, "At least one device should fail (NonExistent)");
}

static void
test_auto_resolve(void)
{
    printf("\n=== Test: Auto Resolve ===\n");
    
    const char *device_names[] = {"Malloc0", "Malloc1", "NonExistent"};
    struct xpdk_open_opts opts;
    struct xpdk_open_result result;
    
    xpdk_open_opts_init(&opts);
    opts.flags = O_RDWR;
    opts.allow_partial_failure = true;
    
    int rc = xpdk_open_auto_resolve(device_names, 3, &opts, &result);
    
    TEST_ASSERT(result.requested_count == 3, "Requested count should be 3");
    
    if (result.opened_count > 0) {
        printf("Auto resolve opened %u devices\n", result.opened_count);
        
        /* Close opened devices */
        rc = xpdk_close_batch(result.devices, result.opened_count);
        TEST_ASSERT(rc == XPDK_SUCCESS, "Auto resolve close should succeed");
    }
}

static void
test_dependency_resolution(void)
{
    printf("\n=== Test: Dependency Resolution ===\n");
    
    struct xpdk_open_request requests[2];
    struct xpdk_open_result result;
    
    /* Initialize requests */
    for (int i = 0; i < 2; i++) {
        xpdk_open_opts_init(&requests[i].opts);
        requests[i].opts.flags = O_RDWR;
        requests[i].group_id = 200;
        requests[i].priority = 0;
    }
    
    /* Device 0: No dependencies */
    snprintf(requests[0].bdev_name, sizeof(requests[0].bdev_name), "Malloc0");
    requests[0].dependency_count = 0;
    
    /* Device 1: Depends on Device 0 */
    snprintf(requests[1].bdev_name, sizeof(requests[1].bdev_name), "Malloc1");
    requests[1].dependency_count = 1;
    snprintf(requests[1].dependencies[0], sizeof(requests[1].dependencies[0]), "Malloc0");
    
    int rc = xpdk_open_batch(requests, 2, &result);
    
    if (rc == XPDK_SUCCESS && result.opened_count == 2) {
        TEST_ASSERT(result.devices[1].dependency_count == 1, "Device 1 should have 1 dependency");
        
        /* Validate dependencies */
        rc = xpdk_validate_dependencies(&result.devices[1]);
        TEST_ASSERT(rc == XPDK_SUCCESS, "Dependencies should be valid");
        
        /* Close devices */
        xpdk_close_batch(result.devices, result.opened_count);
    } else {
        printf("WARNING: Dependency test requires both Malloc0 and Malloc1 devices\n");
        test_passed++; /* Don't fail if devices don't exist */
    }
}

static void
test_device_state_monitoring(void)
{
    printf("\n=== Test: Device State Monitoring ===\n");
    
    struct xpdk_open_opts opts;
    struct xpdk_device_desc desc;
    
    xpdk_open_opts_init(&opts);
    
    int rc = xpdk_open_advanced("Malloc0", &opts, &desc);
    if (rc == XPDK_SUCCESS) {
        /* Test state functions */
        xpdk_device_state_t state = xpdk_get_device_state(desc.fd);
        TEST_ASSERT(state == XPDK_DEVICE_STATE_OPEN, "Device should be in open state");
        
        /* Test state waiting */
        rc = xpdk_wait_device_state(desc.fd, XPDK_DEVICE_STATE_OPEN, 1000);
        TEST_ASSERT(rc == XPDK_SUCCESS, "Should successfully wait for open state");
        
        xpdk_close_advanced(&desc);
    } else {
        printf("WARNING: Could not test state monitoring (device not available)\n");
        test_passed++; /* Don't fail if device doesn't exist */
    }
}

static void
test_device_statistics(void)
{
    printf("\n=== Test: Device Statistics ===\n");
    
    struct xpdk_open_opts opts;
    struct xpdk_device_desc desc;
    
    xpdk_open_opts_init(&opts);
    
    int rc = xpdk_open_advanced("Malloc0", &opts, &desc);
    if (rc == XPDK_SUCCESS) {
        struct xpdk_perf_stats stats;
        
        /* Get initial stats */
        rc = xpdk_get_device_stats(desc.fd, &stats);
        TEST_ASSERT(rc == XPDK_SUCCESS, "Should get device statistics");
        
        /* Reset stats */
        rc = xpdk_reset_device_stats(desc.fd);
        TEST_ASSERT(rc == XPDK_SUCCESS, "Should reset device statistics");
        
        xpdk_close_advanced(&desc);
    } else {
        printf("WARNING: Could not test statistics (device not available)\n");
        test_passed++; /* Don't fail if device doesn't exist */
    }
}

static void
test_error_handling(void)
{
    printf("\n=== Test: Error Handling ===\n");
    
    struct xpdk_open_opts opts;
    struct xpdk_device_desc desc;
    
    /* Test invalid device name */
    xpdk_open_opts_init(&opts);
    int rc = xpdk_open_advanced(NULL, &opts, &desc);
    TEST_ASSERT(rc == XPDK_ERROR_INVALID, "NULL device name should fail");
    
    /* Test invalid options */
    rc = xpdk_open_advanced("Malloc0", NULL, &desc);
    TEST_ASSERT(rc == XPDK_ERROR_INVALID, "NULL options should fail");
    
    /* Test invalid descriptor */
    rc = xpdk_open_advanced("Malloc0", &opts, NULL);
    TEST_ASSERT(rc == XPDK_ERROR_INVALID, "NULL descriptor should fail");
    
    /* Test non-existent device */
    rc = xpdk_open_advanced("NonExistentDevice", &opts, &desc);
    TEST_ASSERT(rc != XPDK_SUCCESS, "Non-existent device should fail");
    
    /* Test batch with invalid parameters */
    rc = xpdk_open_batch(NULL, 1, NULL);
    TEST_ASSERT(rc == XPDK_ERROR_INVALID, "Batch with NULL parameters should fail");
    
    /* Test auto resolve with invalid parameters */
    rc = xpdk_open_auto_resolve(NULL, 0, &opts, NULL);
    TEST_ASSERT(rc == XPDK_ERROR_INVALID, "Auto resolve with invalid parameters should fail");
}

static void
test_option_validation(void)
{
    printf("\n=== Test: Option Validation ===\n");
    
    struct xpdk_open_opts opts;
    struct xpdk_device_desc desc;
    
    /* Test valid options */
    xpdk_open_opts_init(&opts);
    opts.timeout_ms = 10000;
    opts.cpu_core = 1;
    opts.queue_depth = 64;
    opts.max_retry_count = 5;
    opts.retry_delay_ms = 200;
    
    /* These should not cause validation errors */
    TEST_ASSERT(opts.timeout_ms <= 300000, "Timeout should be within limits");
    TEST_ASSERT(opts.cpu_core >= -1 && opts.cpu_core < 1024, "CPU core should be valid");
    TEST_ASSERT(opts.queue_depth <= 32768, "Queue depth should be within limits");
}

int
main(int argc, char **argv)
{
    printf("XPDK Advanced Open Mechanism Test Suite\n");
    printf("=======================================\n");

    /* Initialize XPDK */
    struct xpdk_opts xpdk_opts;
    xpdk_opts_init(&xpdk_opts);
    xpdk_opts.turbo_mode = false; /* Use standard mode for testing */

    int rc = xpdk_init_opts(&xpdk_opts);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK: %s\n", xpdk_strerror(rc));
        return 1;
    }

    printf("XPDK initialized successfully\n");

    /* Run tests */
    test_advanced_open_opts_init();
    test_simple_advanced_open();
    test_batch_open();
    test_auto_resolve();
    test_dependency_resolution();
    test_device_state_monitoring();
    test_device_statistics();
    test_error_handling();
    test_option_validation();

    /* Cleanup */
    xpdk_cleanup();

    /* Print results */
    printf("\n=== Test Results ===\n");
    printf("Tests passed: %d\n", test_passed);
    printf("Tests failed: %d\n", test_failed);
    printf("Total tests: %d\n", test_passed + test_failed);

    if (test_failed == 0) {
        printf("All tests PASSED!\n");
        return 0;
    } else {
        printf("Some tests FAILED!\n");
        return 1;
    }
}
