#include "xpdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    (void)argc; /* unused parameter */
    (void)argv; /* unused parameter */
    
    struct xpdk_opts opts;
    struct xpdk_bdev_info devices[16];
    xpdk_fd_t fd;
    int rc, num_devices;
    
    printf("=== XPDK Vectored I/O Example ===\n");
    
    /* Initialize XPDK */
    xpdk_opts_init(&opts);
    rc = xpdk_init_opts(&opts);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK: %s\n", xpdk_strerror(rc));
        return 1;
    }
    
    /* List available devices */
    num_devices = xpdk_list_bdevs(devices, 16);
    if (num_devices <= 0) {
        printf("No block devices found\n");
        xpdk_cleanup();
        return 1;
    }
    
    printf("Found %d devices, using: %s\n", num_devices, devices[0].name);
    
    /* Open the first device */
    fd = xpdk_open(devices[0].name, O_RDWR);
    if (fd < 0) {
        printf("Failed to open device: %s\n", xpdk_strerror(fd));
        xpdk_cleanup();
        return 1;
    }
    
    /* Prepare vectored write data */
    printf("\n=== Vectored Write Test ===\n");
    
    char buffer1[1024];
    char buffer2[2048];  
    char buffer3[512];
    
    memset(buffer1, 'A', sizeof(buffer1));
    memset(buffer2, 'B', sizeof(buffer2));
    memset(buffer3, 'C', sizeof(buffer3));
    
    struct xpdk_iovec write_iov[3] = {
        { .iov_base = buffer1, .iov_len = sizeof(buffer1) },
        { .iov_base = buffer2, .iov_len = sizeof(buffer2) },
        { .iov_base = buffer3, .iov_len = sizeof(buffer3) }
    };
    
    printf("Writing vectored data: %zu + %zu + %zu = %zu bytes\n", 
           write_iov[0].iov_len, write_iov[1].iov_len, write_iov[2].iov_len,
           write_iov[0].iov_len + write_iov[1].iov_len + write_iov[2].iov_len);
    
    ssize_t bytes_written = xpdk_writev(fd, write_iov, 3, 0);
    if (bytes_written < 0) {
        printf("Vectored write failed: %s\n", xpdk_strerror((int)bytes_written));
    } else {
        printf("Vectored write successful: %zd bytes\n", bytes_written);
    }
    
    /* Prepare vectored read data */
    printf("\n=== Vectored Read Test ===\n");
    
    char read_buffer1[1024];
    char read_buffer2[2048];
    char read_buffer3[512];
    
    memset(read_buffer1, 0, sizeof(read_buffer1));
    memset(read_buffer2, 0, sizeof(read_buffer2));
    memset(read_buffer3, 0, sizeof(read_buffer3));
    
    struct xpdk_iovec read_iov[3] = {
        { .iov_base = read_buffer1, .iov_len = sizeof(read_buffer1) },
        { .iov_base = read_buffer2, .iov_len = sizeof(read_buffer2) },
        { .iov_base = read_buffer3, .iov_len = sizeof(read_buffer3) }
    };
    
    ssize_t bytes_read = xpdk_readv(fd, read_iov, 3, 0);
    if (bytes_read < 0) {
        printf("Vectored read failed: %s\n", xpdk_strerror((int)bytes_read));
    } else {
        printf("Vectored read successful: %zd bytes\n", bytes_read);
        
        /* Verify data integrity */
        bool verify_ok = true;
        
        for (size_t i = 0; i < sizeof(read_buffer1); i++) {
            if (read_buffer1[i] != 'A') {
                verify_ok = false;
                break;
            }
        }
        
        for (size_t i = 0; i < sizeof(read_buffer2); i++) {
            if (read_buffer2[i] != 'B') {
                verify_ok = false;
                break;
            }
        }
        
        for (size_t i = 0; i < sizeof(read_buffer3); i++) {
            if (read_buffer3[i] != 'C') {
                verify_ok = false;
                break;
            }
        }
        
        printf("Data integrity check: %s\n", verify_ok ? "PASSED" : "FAILED");
    }
    
    /* Test performance statistics */
    printf("\n=== Performance Statistics ===\n");
    struct xpdk_perf_stats stats;
    rc = xpdk_get_perf_stats(fd, &stats);
    if (rc == XPDK_SUCCESS) {
        printf("Performance Statistics:\n");
        printf("  Total Read Ops:    %lu\n", stats.total_read_ops);
        printf("  Total Write Ops:   %lu\n", stats.total_write_ops);
        printf("  Total Bytes Read:  %lu\n", stats.total_bytes_read);
        printf("  Total Bytes Written: %lu\n", stats.total_bytes_written);
        printf("  Avg Read Latency:  %lu us\n", stats.avg_read_latency_us);
        printf("  Avg Write Latency: %lu us\n", stats.avg_write_latency_us);
        printf("  Current IOPS:      %lu\n", stats.current_iops);
        printf("  Current Bandwidth: %lu bytes/sec\n", stats.current_bandwidth);
        printf("  Errors:            %lu\n", stats.errors);
    } else {
        printf("Failed to get performance statistics: %s\n", xpdk_strerror(rc));
    }
    
    /* Test buffer allocation */
    printf("\n=== Buffer Allocation Test ===\n");
    void *aligned_buffer = NULL;
    int alloc_rc = posix_memalign(&aligned_buffer, 4096, 8192);
    if (alloc_rc == 0 && aligned_buffer) {
        printf("Successfully allocated 8KB aligned buffer\n");
        
        /* Test the buffer */
        memset(aligned_buffer, 0xAA, 8192);
        printf("Buffer test: filled with pattern\n");
        
        free(aligned_buffer);
        printf("Buffer freed successfully\n");
    } else {
        printf("Failed to allocate aligned buffer\n");
    }
    
    /* Test trim operation */
    printf("\n=== Trim Operation Test ===\n");
    rc = xpdk_trim(fd, 0, 4096);
    if (rc == XPDK_SUCCESS) {
        printf("Trim operation successful\n");
    } else {
        printf("Trim operation failed: %s\n", xpdk_strerror(rc));
    }
    
    /* Close device */
    rc = xpdk_close(fd);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to close device: %s\n", xpdk_strerror(rc));
    }
    
    /* Cleanup */
    xpdk_cleanup();
    
    printf("\nVectored I/O example completed successfully\n");
    return 0;
}
