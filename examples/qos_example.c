#include "xpdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    struct xpdk_opts opts;
    struct xpdk_bdev_info devices[16];
    xpdk_fd_t fd;
    uint64_t qos_limits[XPDK_QOS_NUM_RATE_LIMIT_TYPES];
    char buffer[4096];
    int rc, num_devices;
    
    printf("=== XPDK QoS Example ===\n");
    
    /* Initialize XPDK with default options */
    xpdk_opts_init(&opts);
    opts.turbo_mode = false;  /* Use standard mode for this example */
    
    rc = xpdk_init_opts(&opts);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK: %s\n", xpdk_strerror(rc));
        return 1;
    }
    
    /* List available block devices */
    num_devices = xpdk_list_bdevs(devices, 16);
    if (num_devices <= 0) {
        printf("No block devices found\n");
        xpdk_cleanup();
        return 1;
    }
    
    printf("Found %d block devices:\n", num_devices);
    for (int i = 0; i < num_devices; i++) {
        printf("  %s: %lu blocks, %lu bytes each\n", 
               devices[i].name, devices[i].num_blocks, devices[i].block_size);
    }
    
    /* Open the first device */
    printf("\nOpening device: %s\n", devices[0].name);
    fd = xpdk_open(devices[0].name, O_RDWR);
    if (fd < 0) {
        printf("Failed to open device %s: %s\n", devices[0].name, xpdk_strerror(fd));
        xpdk_cleanup();
        return 1;
    }
    
    /* Get current QoS limits (should be all zeros initially) */
    printf("\nGetting current QoS limits...\n");
    rc = xpdk_qos_get_rate_limits(fd, qos_limits);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to get QoS limits: %s\n", xpdk_strerror(rc));
    } else {
        printf("Current QoS limits:\n");
        printf("  RW IOPS: %lu\n", qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT]);
        printf("  RW BPS:  %lu\n", qos_limits[XPDK_QOS_RW_BPS_RATE_LIMIT]);
        printf("  R BPS:   %lu\n", qos_limits[XPDK_QOS_R_BPS_RATE_LIMIT]);
        printf("  W BPS:   %lu\n", qos_limits[XPDK_QOS_W_BPS_RATE_LIMIT]);
    }
    
    /* Set QoS limits using simple interface */
    printf("\nSetting QoS limits: 1000 IOPS, 4MB/s bandwidth\n");
    rc = xpdk_qos_enable_simple(fd, 1000, 4 * 1024 * 1024);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to set QoS limits: %s\n", xpdk_strerror(rc));
    } else {
        printf("QoS limits set successfully\n");
    }
    
    /* Verify the limits were set */
    printf("\nVerifying QoS limits...\n");
    rc = xpdk_qos_get_rate_limits(fd, qos_limits);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to get QoS limits: %s\n", xpdk_strerror(rc));
    } else {
        printf("Updated QoS limits:\n");
        printf("  RW IOPS: %lu\n", qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT]);
        printf("  RW BPS:  %lu\n", qos_limits[XPDK_QOS_RW_BPS_RATE_LIMIT]);
        printf("  R BPS:   %lu\n", qos_limits[XPDK_QOS_R_BPS_RATE_LIMIT]);
        printf("  W BPS:   %lu\n", qos_limits[XPDK_QOS_W_BPS_RATE_LIMIT]);
    }
    
    /* Perform some I/O operations to test QoS */
    printf("\nPerforming I/O operations with QoS limits...\n");
    memset(buffer, 0xAB, sizeof(buffer));
    
    /* Write some data */
    printf("Writing 4KB of data...\n");
    ssize_t bytes_written = xpdk_write(fd, buffer, sizeof(buffer), 0);
    if (bytes_written != sizeof(buffer)) {
        printf("Write failed: %s\n", xpdk_strerror((int)bytes_written));
    } else {
        printf("Write successful: %zd bytes\n", bytes_written);
    }
    
    /* Read the data back */
    printf("Reading 4KB of data...\n");
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = xpdk_read(fd, buffer, sizeof(buffer), 0);
    if (bytes_read != sizeof(buffer)) {
        printf("Read failed: %s\n", xpdk_strerror((int)bytes_read));
    } else {
        printf("Read successful: %zd bytes\n", bytes_read);
    }
    
    /* Set custom QoS limits using advanced interface */
    printf("\nSetting custom QoS limits...\n");
    memset(qos_limits, 0, sizeof(qos_limits));
    qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT] = 500;           /* 500 IOPS for R/W */
    qos_limits[XPDK_QOS_R_BPS_RATE_LIMIT] = 2 * 1024 * 1024; /* 2MB/s for reads */
    qos_limits[XPDK_QOS_W_BPS_RATE_LIMIT] = 1 * 1024 * 1024; /* 1MB/s for writes */
    
    rc = xpdk_qos_set_rate_limits(fd, qos_limits);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to set custom QoS limits: %s\n", xpdk_strerror(rc));
    } else {
        printf("Custom QoS limits set successfully\n");
        
        /* Verify the custom limits */
        memset(qos_limits, 0, sizeof(qos_limits));
        rc = xpdk_qos_get_rate_limits(fd, qos_limits);
        if (rc == XPDK_SUCCESS) {
            printf("Custom QoS limits:\n");
            printf("  RW IOPS: %lu\n", qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT]);
            printf("  RW BPS:  %lu\n", qos_limits[XPDK_QOS_RW_BPS_RATE_LIMIT]);
            printf("  R BPS:   %lu\n", qos_limits[XPDK_QOS_R_BPS_RATE_LIMIT]);
            printf("  W BPS:   %lu\n", qos_limits[XPDK_QOS_W_BPS_RATE_LIMIT]);
        }
    }
    
    /* Disable QoS */
    printf("\nDisabling QoS limits...\n");
    rc = xpdk_qos_disable(fd);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to disable QoS: %s\n", xpdk_strerror(rc));
    } else {
        printf("QoS disabled successfully\n");
        
        /* Verify QoS is disabled */
        memset(qos_limits, 0, sizeof(qos_limits));
        rc = xpdk_qos_get_rate_limits(fd, qos_limits);
        if (rc == XPDK_SUCCESS) {
            printf("QoS limits after disable:\n");
            printf("  RW IOPS: %lu\n", qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT]);
            printf("  RW BPS:  %lu\n", qos_limits[XPDK_QOS_RW_BPS_RATE_LIMIT]);
            printf("  R BPS:   %lu\n", qos_limits[XPDK_QOS_R_BPS_RATE_LIMIT]);
            printf("  W BPS:   %lu\n", qos_limits[XPDK_QOS_W_BPS_RATE_LIMIT]);
        }
    }
    
    /* Close the device */
    rc = xpdk_close(fd);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to close device: %s\n", xpdk_strerror(rc));
    }
    
    /* Cleanup */
    xpdk_cleanup();
    
    printf("\nQoS example completed successfully\n");
    return 0;
}
