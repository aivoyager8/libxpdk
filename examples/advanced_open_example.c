#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "xpdk.h"
#include "xpdk_advanced.h"

static void
print_device_info(const struct xpdk_device_desc *desc)
{
    printf("Device: %s\n", desc->name);
    printf("  FD: %d\n", desc->fd);
    printf("  State: %d\n", desc->state);
    printf("  Group ID: %u\n", desc->group_id);
    printf("  Block Size: %lu bytes\n", desc->info.block_size);
    printf("  Capacity: %lu bytes (%.2f GB)\n", 
           desc->info.capacity, (double)desc->info.capacity / (1024*1024*1024));
    printf("  Dependencies: %u\n", desc->dependency_count);
    for (uint32_t i = 0; i < desc->dependency_count; i++) {
        printf("    - FD %d\n", desc->dependencies[i]);
    }
    printf("\n");
}

static void
example_simple_advanced_open(void)
{
    printf("=== Example 1: Simple Advanced Open ===\n");
    
    struct xpdk_open_opts opts;
    struct xpdk_device_desc desc;

    xpdk_open_opts_init(&opts);
    opts.flags = O_RDWR;
    opts.enable_turbo = true;
    opts.cpu_core = 2;
    opts.queue_depth = 128;
    opts.timeout_ms = 10000; /* 10 second timeout */
    opts.enable_auto_retry = true;
    opts.max_retry_count = 3;

    int rc = xpdk_open_advanced("Malloc0", &opts, &desc);
    if (rc == XPDK_SUCCESS) {
        printf("Successfully opened device with advanced options:\n");
        print_device_info(&desc);
        
        /* Use the device... */
        char buffer[4096];
        ssize_t bytes_read = xpdk_read(desc.fd, buffer, sizeof(buffer), 0);
        printf("Read %ld bytes from device\n", bytes_read);
        
        xpdk_close_advanced(&desc);
        printf("Device closed successfully\n");
    } else {
        printf("Failed to open device: %s\n", xpdk_strerror(rc));
    }
    
    printf("\n");
}

static void
example_batch_open(void)
{
    printf("=== Example 2: Batch Device Opening ===\n");
    
    struct xpdk_open_request requests[3];
    struct xpdk_open_result result;

    /* Initialize requests */
    for (int i = 0; i < 3; i++) {
        xpdk_open_opts_init(&requests[i].opts);
        requests[i].opts.flags = O_RDWR;
        requests[i].opts.allow_partial_failure = true; /* Allow partial success */
        requests[i].group_id = 1; /* Put all devices in group 1 */
        requests[i].dependency_count = 0;
        requests[i].priority = 0;
    }

    /* Configure device names */
    snprintf(requests[0].bdev_name, sizeof(requests[0].bdev_name), "Malloc0");
    snprintf(requests[1].bdev_name, sizeof(requests[1].bdev_name), "Malloc1");
    snprintf(requests[2].bdev_name, sizeof(requests[2].bdev_name), "Malloc2");

    /* Open all devices in batch */
    int rc = xpdk_open_batch(requests, 3, &result);
    printf("Batch open result: %s\n", xpdk_strerror(rc));
    printf("Requested: %u, Opened: %u, Failed: %u\n", 
           result.requested_count, result.opened_count, result.failed_count);
    printf("Operation time: %lu microseconds\n", result.operation_time_us);
    
    if (result.failed_count > 0) {
        printf("Error details: %s\n", result.error_details);
    }

    /* Print information for successfully opened devices */
    for (uint32_t i = 0; i < result.opened_count; i++) {
        printf("Opened device %u:\n", i);
        print_device_info(&result.devices[i]);
    }

    /* Close all devices */
    if (result.opened_count > 0) {
        rc = xpdk_close_batch(result.devices, result.opened_count);
        printf("Batch close result: %s\n", xpdk_strerror(rc));
    }
    
    printf("\n");
}

static void
example_dependency_resolution(void)
{
    printf("=== Example 3: Dependency Resolution ===\n");
    
    struct xpdk_open_request requests[3];
    struct xpdk_open_result result;

    /* Initialize requests with dependencies */
    for (int i = 0; i < 3; i++) {
        xpdk_open_opts_init(&requests[i].opts);
        requests[i].opts.flags = O_RDWR;
        requests[i].group_id = 2;
        requests[i].priority = 0;
    }

    /* Device 0: No dependencies (base device) */
    snprintf(requests[0].bdev_name, sizeof(requests[0].bdev_name), "Malloc0");
    requests[0].dependency_count = 0;

    /* Device 1: Depends on Device 0 */
    snprintf(requests[1].bdev_name, sizeof(requests[1].bdev_name), "Malloc1");
    requests[1].dependency_count = 1;
    snprintf(requests[1].dependencies[0], sizeof(requests[1].dependencies[0]), "Malloc0");

    /* Device 2: Depends on Device 1 */
    snprintf(requests[2].bdev_name, sizeof(requests[2].bdev_name), "Malloc2");
    requests[2].dependency_count = 1;
    snprintf(requests[2].dependencies[0], sizeof(requests[2].dependencies[0]), "Malloc1");

    /* Open with dependency resolution */
    int rc = xpdk_open_batch(requests, 3, &result);
    printf("Dependency resolution result: %s\n", xpdk_strerror(rc));
    
    if (rc == XPDK_SUCCESS) {
        printf("Successfully opened %u devices with dependencies:\n", result.opened_count);
        for (uint32_t i = 0; i < result.opened_count; i++) {
            print_device_info(&result.devices[i]);
            
            /* Validate dependencies */
            rc = xpdk_validate_dependencies(&result.devices[i]);
            printf("Dependency validation: %s\n", 
                   rc == XPDK_SUCCESS ? "PASSED" : "FAILED");
        }
        
        /* Close all devices */
        xpdk_close_batch(result.devices, result.opened_count);
    } else {
        printf("Failed to resolve dependencies: %s\n", result.error_details);
    }
    
    printf("\n");
}

static void
example_auto_resolve(void)
{
    printf("=== Example 4: Auto Resolve Open ===\n");
    
    const char *device_names[] = {"Malloc0", "Malloc1", "Malloc2"};
    struct xpdk_open_opts opts;
    struct xpdk_open_result result;

    xpdk_open_opts_init(&opts);
    opts.flags = O_RDWR;
    opts.enable_turbo = false;
    opts.allow_partial_failure = true;

    int rc = xpdk_open_auto_resolve(device_names, 3, &opts, &result);
    printf("Auto resolve result: %s\n", xpdk_strerror(rc));
    printf("Opened %u out of %u devices\n", result.opened_count, result.requested_count);

    if (result.opened_count > 0) {
        /* Perform some I/O operations */
        for (uint32_t i = 0; i < result.opened_count; i++) {
            struct xpdk_perf_stats stats;
            rc = xpdk_get_device_stats(result.devices[i].fd, &stats);
            if (rc == XPDK_SUCCESS) {
                printf("Device %s stats: Read Ops=%lu, Write Ops=%lu\n",
                       result.devices[i].name, stats.total_read_ops, stats.total_write_ops);
            }
        }
        
        /* Close devices */
        xpdk_close_batch(result.devices, result.opened_count);
    }
    
    printf("\n");
}

static void
example_device_state_monitoring(void)
{
    printf("=== Example 5: Device State Monitoring ===\n");
    
    struct xpdk_open_opts opts;
    struct xpdk_device_desc desc;

    xpdk_open_opts_init(&opts);
    opts.flags = O_RDWR;

    int rc = xpdk_open_advanced("Malloc0", &opts, &desc);
    if (rc == XPDK_SUCCESS) {
        printf("Device opened, monitoring state...\n");
        
        /* Check current state */
        xpdk_device_state_t state = xpdk_get_device_state(desc.fd);
        printf("Current state: %d\n", state);
        
        /* Wait for device to be ready (should be immediate) */
        rc = xpdk_wait_device_state(desc.fd, XPDK_DEVICE_STATE_OPEN, 5000);
        if (rc == XPDK_SUCCESS) {
            printf("Device is ready for I/O\n");
        } else {
            printf("Device not ready within timeout\n");
        }
        
        xpdk_close_advanced(&desc);
        printf("Device closed\n");
    } else {
        printf("Failed to open device for monitoring: %s\n", xpdk_strerror(rc));
    }
    
    printf("\n");
}

static void
example_performance_comparison(void)
{
    printf("=== Example 6: Performance Comparison ===\n");
    
    struct xpdk_open_opts standard_opts, turbo_opts;
    struct xpdk_device_desc standard_desc, turbo_desc;
    
    /* Standard mode */
    xpdk_open_opts_init(&standard_opts);
    standard_opts.flags = O_RDWR;
    standard_opts.enable_turbo = false;
    
    /* Turbo mode */
    xpdk_open_opts_init(&turbo_opts);
    turbo_opts.flags = O_RDWR;
    turbo_opts.enable_turbo = true;
    turbo_opts.cpu_core = 1;
    turbo_opts.queue_depth = 256;
    
    /* Open devices */
    int rc1 = xpdk_open_advanced("Malloc0", &standard_opts, &standard_desc);
    int rc2 = xpdk_open_advanced("Malloc1", &turbo_opts, &turbo_desc);
    
    if (rc1 == XPDK_SUCCESS && rc2 == XPDK_SUCCESS) {
        printf("Both devices opened successfully\n");
        printf("Standard mode device: %s (FD %d)\n", standard_desc.name, standard_desc.fd);
        printf("Turbo mode device: %s (FD %d)\n", turbo_desc.name, turbo_desc.fd);
        
        /* Perform simple I/O test */
        char buffer[4096];
        memset(buffer, 0xAA, sizeof(buffer));
        
        printf("Testing standard mode device...\n");
        ssize_t written = xpdk_write(standard_desc.fd, buffer, sizeof(buffer), 0);
        ssize_t read = xpdk_read(standard_desc.fd, buffer, sizeof(buffer), 0);
        printf("Standard: Written %ld bytes, Read %ld bytes\n", written, read);
        
        printf("Testing turbo mode device...\n");
        written = xpdk_write(turbo_desc.fd, buffer, sizeof(buffer), 0);
        read = xpdk_read(turbo_desc.fd, buffer, sizeof(buffer), 0);
        printf("Turbo: Written %ld bytes, Read %ld bytes\n", written, read);
        
        /* Close devices */
        xpdk_close_advanced(&standard_desc);
        xpdk_close_advanced(&turbo_desc);
        printf("Both devices closed\n");
    } else {
        printf("Failed to open devices for performance comparison\n");
        if (rc1 != XPDK_SUCCESS) printf("Standard mode error: %s\n", xpdk_strerror(rc1));
        if (rc2 != XPDK_SUCCESS) printf("Turbo mode error: %s\n", xpdk_strerror(rc2));
    }
    
    printf("\n");
}

int
main(int argc, char **argv)
{
    (void)argc; /* unused parameter */
    (void)argv; /* unused parameter */
    
    printf("XPDK Advanced Open Mechanism Examples\n");
    printf("=====================================\n\n");

    /* Initialize XPDK with turbo mode */
    struct xpdk_opts xpdk_opts;
    xpdk_opts_init(&xpdk_opts);
    xpdk_opts.turbo_mode = true;
    xpdk_opts.cpu_core = 0;

    int rc = xpdk_init_opts(&xpdk_opts);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK: %s\n", xpdk_strerror(rc));
        return 1;
    }

    printf("XPDK initialized successfully\n\n");

    /* Run examples */
    example_simple_advanced_open();
    example_batch_open();
    example_dependency_resolution();
    example_auto_resolve();
    example_device_state_monitoring();
    example_performance_comparison();

    /* Cleanup */
    xpdk_cleanup();
    printf("XPDK cleanup completed\n");

    return 0;
}
