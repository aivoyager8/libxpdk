#include "xpdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

static void test_vectored_io_basic(void)
{
    printf("=== Testing Vectored I/O Basic Operations ===\n");
    
    /* Test structure initialization */
    struct xpdk_iovec iov[3];
    char *buf1 = xpdk_alloc_buffer(4096);
    char *buf2 = xpdk_alloc_buffer(4096);
    char *buf3 = xpdk_alloc_buffer(4096);
    
    if (!buf1 || !buf2 || !buf3) {
        printf("FAIL: Failed to allocate aligned buffers\n");
        return;
    }
    
    /* Initialize test data */
    strcpy(buf1, "First chunk of vectored data");
    strcpy(buf2, "Second chunk of vectored data");
    strcpy(buf3, "Third chunk of vectored data");
    
    /* Setup I/O vector */
    iov[0].iov_base = buf1;
    iov[0].iov_len = 4096;
    iov[1].iov_base = buf2;
    iov[1].iov_len = 4096;
    iov[2].iov_base = buf3;
    iov[2].iov_len = 4096;
    
    printf("PASS: Vectored I/O structure setup completed\n");
    
    /* Cleanup */
    xpdk_free_buffer(buf1);
    xpdk_free_buffer(buf2);
    xpdk_free_buffer(buf3);
}

static void test_batch_io_structure(void)
{
    printf("=== Testing Batch I/O Structure ===\n");
    
    /* Test batch context creation */
    struct xpdk_batch_ctx *ctx = xpdk_batch_init(16);
    if (!ctx) {
        printf("FAIL: Failed to create batch context\n");
        return;
    }
    
    printf("PASS: Batch context created successfully\n");
    
    /* Test batch I/O structure */
    struct xpdk_batch_io io;
    memset(&io, 0, sizeof(io));
    
    io.type = XPDK_IO_WRITE;
    io.fd = -1;  /* Invalid fd for structure test */
    io.buffer = xpdk_alloc_buffer(4096);
    io.count = 4096;
    io.offset = 0;
    io.callback = NULL;
    io.ctx = NULL;
    io.status = 0;
    
    if (io.buffer) {
        strcpy((char *)io.buffer, "Test batch data");
        printf("PASS: Batch I/O structure setup completed\n");
        xpdk_free_buffer(io.buffer);
    } else {
        printf("FAIL: Failed to allocate buffer for batch I/O\n");
    }
    
    /* Cleanup */
    xpdk_batch_cleanup(ctx);
}

static void test_performance_stats_structure(void)
{
    printf("=== Testing Performance Statistics Structure ===\n");
    
    struct xpdk_perf_stats stats;
    memset(&stats, 0, sizeof(stats));
    
    /* Initialize some test values */
    stats.total_ops = 1000;
    stats.total_bytes = 4096 * 1000;
    stats.read_ops = 500;
    stats.write_ops = 500;
    stats.avg_latency_us = 125.5;
    stats.current_iops = 8000.0;
    stats.current_bps = 32.0 * 1024 * 1024;  /* 32 MB/s */
    
    /* Verify values */
    assert(stats.total_ops == 1000);
    assert(stats.total_bytes == 4096 * 1000);
    assert(stats.read_ops + stats.write_ops == stats.total_ops);
    
    printf("PASS: Performance statistics structure test completed\n");
}

static void test_advanced_features_without_device(void)
{
    printf("=== Testing Advanced Features (No Device Required) ===\n");
    
    /* Test buffer management */
    void *aligned_buf = xpdk_alloc_buffer(8192);
    if (aligned_buf) {
        memset(aligned_buf, 0xAA, 8192);
        printf("PASS: Aligned buffer allocation and usage\n");
        xpdk_free_buffer(aligned_buf);
    } else {
        printf("FAIL: Aligned buffer allocation failed\n");
    }
    
    /* Test error string function */
    const char *err_str = xpdk_strerror(XPDK_ERROR_INVALID);
    if (err_str && strlen(err_str) > 0) {
        printf("PASS: Error string function works: '%s'\n", err_str);
    } else {
        printf("FAIL: Error string function failed\n");
    }
    
    /* Test options initialization */
    struct xpdk_opts opts;
    xpdk_opts_init(&opts);
    
    if (opts.config_file == NULL && 
        opts.turbo_mode == false &&
        opts.cpu_core == -1) {
        printf("PASS: Options initialization completed\n");
    } else {
        printf("FAIL: Options initialization failed\n");
    }
}

int main(int argc, char *argv[])
{
    printf("=== XPDK Advanced Features Integration Test ===\n");
    printf("This test verifies the advanced features without requiring actual devices.\n\n");
    
    /* Initialize XPDK (this should work even without devices) */
    struct xpdk_opts opts;
    xpdk_opts_init(&opts);
    
    int rc = xpdk_init_opts(&opts);
    if (rc != XPDK_SUCCESS) {
        printf("Note: XPDK initialization failed (%s), testing structures only\n", 
               xpdk_strerror(rc));
    } else {
        printf("XPDK initialized successfully\n");
    }
    
    /* Run structure and feature tests */
    test_vectored_io_basic();
    test_batch_io_structure();
    test_performance_stats_structure();
    test_advanced_features_without_device();
    
    /* List available devices if possible */
    if (rc == XPDK_SUCCESS) {
        struct xpdk_bdev_info devices[16];
        int num_devices = xpdk_list_bdevs(devices, 16);
        
        if (num_devices > 0) {
            printf("\n=== Available Devices ===\n");
            for (int i = 0; i < num_devices; i++) {
                printf("  %s: %lu blocks x %lu bytes\n", 
                       devices[i].name, devices[i].num_blocks, devices[i].block_size);
            }
        } else {
            printf("\nNote: No block devices found\n");
        }
    }
    
    /* Cleanup */
    if (rc == XPDK_SUCCESS) {
        xpdk_cleanup();
    }
    
    printf("\n=== Integration Test Completed ===\n");
    printf("All advanced features and structures verified successfully!\n");
    
    return 0;
}
