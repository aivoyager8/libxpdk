#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "xpdk.h"

int main(int argc, char *argv[])
{
    struct xpdk_opts opts;
    xpdk_fd_t fd;
    char write_data[4096];
    char read_data[4096];
    struct xpdk_bdev_info info;
    int rc;
    bool use_turbo = false;
    int cpu_core = -1;

    /* Parse command line arguments */
    if (argc < 2) {
        printf("Usage: %s <bdev_name> [--turbo] [--cpu-core <core>]\n", argv[0]);
        printf("  --turbo        Enable turbo mode for maximum performance\n");
        printf("  --cpu-core     Bind SPDK thread to specific CPU core\n");
        return 1;
    }

    const char *bdev_name = argv[1];
    
    /* Parse options */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--turbo") == 0) {
            use_turbo = true;
        } else if (strcmp(argv[i], "--cpu-core") == 0 && i + 1 < argc) {
            cpu_core = atoi(argv[i + 1]);
            i++; /* Skip next argument */
        }
    }

    /* Initialize XPDK options */
    xpdk_opts_init(&opts);
    opts.turbo_mode = use_turbo;
    opts.cpu_core = cpu_core;
    
    /* In turbo mode, use larger buffers for better performance */
    if (use_turbo) {
        opts.msg_ring_size = 2048;
        opts.poll_period_us = 0;  /* Busy polling */
    }
    
    printf("Initializing XPDK library...\n");
    printf("  Mode: %s\n", use_turbo ? "TURBO (High Performance)" : "STANDARD");
    printf("  CPU Core: %s\n", cpu_core >= 0 ? "Bound" : "Unbound");
    
    rc = xpdk_init_opts(&opts);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK: %s\n", xpdk_strerror(rc));
        return 1;
    }

    printf("Opening device: %s\n", bdev_name);
    fd = xpdk_open(bdev_name, O_RDWR);
    if (fd < 0) {
        printf("Failed to open device %s: %s\n", bdev_name, xpdk_strerror(fd));
        xpdk_cleanup();
        return 1;
    }

    /* Get device information */
    rc = xpdk_get_info(fd, &info);
    if (rc == XPDK_SUCCESS) {
        printf("Device info:\n");
        printf("  Name: %s\n", info.name);
        printf("  Block size: %lu bytes\n", info.block_size);
        printf("  Number of blocks: %lu\n", info.num_blocks);
        printf("  Total capacity: %lu bytes (%.2f MB)\n", 
               info.capacity, (double)info.capacity / (1024 * 1024));
    }

    /* Prepare test data */
    memset(write_data, 0xAA, sizeof(write_data));
    snprintf(write_data, sizeof(write_data), 
             "Hello from XPDK %s mode! This is a test write operation.", 
             use_turbo ? "TURBO" : "STANDARD");

    printf("Writing test data...\n");
    ssize_t bytes_written = xpdk_write(fd, write_data, sizeof(write_data), 0);
    if (bytes_written < 0) {
        printf("Write failed: %s\n", xpdk_strerror((int)bytes_written));
    } else {
        printf("Successfully wrote %zd bytes\n", bytes_written);
    }

    /* Flush to ensure data is written */
    printf("Flushing data...\n");
    rc = xpdk_flush(fd);
    if (rc != XPDK_SUCCESS) {
        printf("Flush failed: %s\n", xpdk_strerror(rc));
    }

    /* Read back the data */
    printf("Reading data back...\n");
    memset(read_data, 0, sizeof(read_data));
    ssize_t bytes_read = xpdk_read(fd, read_data, sizeof(read_data), 0);
    if (bytes_read < 0) {
        printf("Read failed: %s\n", xpdk_strerror((int)bytes_read));
    } else {
        printf("Successfully read %zd bytes\n", bytes_read);
        
        /* Verify data */
        if (memcmp(write_data, read_data, sizeof(write_data)) == 0) {
            printf("Data verification successful!\n");
        } else {
            printf("Data verification failed!\n");
        }
    }

    printf("Closing device...\n");
    rc = xpdk_close(fd);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to close device: %s\n", xpdk_strerror(rc));
    }

    printf("Cleaning up...\n");
    xpdk_cleanup();

    printf("Example completed successfully!\n");
    
    if (use_turbo) {
        printf("\nTurbo mode notes:\n");
        printf("- Uses busy polling for minimum latency\n");
        printf("- Higher CPU usage but better performance\n");
        printf("- Ideal for latency-sensitive applications\n");
    } else {
        printf("\nStandard mode notes:\n");
        printf("- Uses periodic polling to reduce CPU usage\n");
        printf("- Good balance of performance and resource usage\n");
        printf("- Suitable for most applications\n");
    }
    
    return 0;
}
