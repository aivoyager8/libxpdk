#include "xpdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

/* Global counters for completion tracking */
static volatile int completed_ops = 0;
static volatile int failed_ops = 0;

/* Batch I/O completion callback */
static void batch_completion_callback(void *ctx, int status)
{
    int *op_id = (int *)ctx;
    if (status == XPDK_SUCCESS) {
        printf("Batch operation %d completed successfully\n", *op_id);
        __sync_fetch_and_add(&completed_ops, 1);
    } else {
        printf("Batch operation %d failed: %s\n", *op_id, xpdk_strerror(status));
        __sync_fetch_and_add(&failed_ops, 1);
    }
}

/* Performance measurement helpers */
static uint64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

static void demonstrate_batch_io(xpdk_fd_t fd)
{
    const int BATCH_SIZE = 16;
    const int BLOCK_SIZE = 4096;
    const int NUM_BATCHES = 4;
    
    printf("\n=== Batch I/O Demonstration ===\n");
    
    /* Allocate buffers */
    char **write_buffers = malloc(BATCH_SIZE * sizeof(char *));
    char **read_buffers = malloc(BATCH_SIZE * sizeof(char *));
    int *op_ids = malloc(BATCH_SIZE * sizeof(int));
    
    for (int i = 0; i < BATCH_SIZE; i++) {
        write_buffers[i] = xpdk_alloc_buffer(BLOCK_SIZE);
        read_buffers[i] = xpdk_alloc_buffer(BLOCK_SIZE);
        
        if (!write_buffers[i] || !read_buffers[i]) {
            printf("Failed to allocate aligned buffers\n");
            return;
        }
        
        /* Fill write buffer with test pattern */
        snprintf(write_buffers[i], BLOCK_SIZE, "Batch write operation %d - test data", i);
        op_ids[i] = i;
    }
    
    /* Initialize batch context */
    struct xpdk_batch_ctx *batch_ctx = xpdk_batch_init(BATCH_SIZE);
    if (!batch_ctx) {
        printf("Failed to initialize batch context\n");
        return;
    }
    
    uint64_t start_time = get_time_us();
    
    for (int batch = 0; batch < NUM_BATCHES; batch++) {
        printf("\nSubmitting batch %d/%d...\n", batch + 1, NUM_BATCHES);
        
        /* Reset counters for this batch */
        completed_ops = 0;
        failed_ops = 0;
        
        /* Prepare batch operations */
        struct xpdk_batch_io ios[BATCH_SIZE];
        for (int i = 0; i < BATCH_SIZE; i++) {
            ios[i].type = XPDK_IO_WRITE;
            ios[i].fd = fd;
            ios[i].buffer = write_buffers[i];
            ios[i].count = BLOCK_SIZE;
            ios[i].offset = (batch * BATCH_SIZE + i) * BLOCK_SIZE;
            ios[i].callback = batch_completion_callback;
            ios[i].ctx = &op_ids[i];
            ios[i].status = 0;
        }
        
        /* Submit the batch */
        int rc = xpdk_batch_submit(batch_ctx, ios, BATCH_SIZE, NULL);
        if (rc != XPDK_SUCCESS) {
            printf("Failed to submit batch: %s\n", xpdk_strerror(rc));
            continue;
        }
        
        /* Wait for all operations in this batch to complete */
        while ((completed_ops + failed_ops) < BATCH_SIZE) {
            xpdk_poll(10);
            usleep(100);
        }
        
        printf("Batch %d completed: %d success, %d failed\n", 
               batch + 1, completed_ops, failed_ops);
    }
    
    uint64_t end_time = get_time_us();
    uint64_t total_time = end_time - start_time;
    
    printf("\nBatch I/O Performance:\n");
    printf("Total operations: %d\n", NUM_BATCHES * BATCH_SIZE);
    printf("Total time: %.2f ms\n", total_time / 1000.0);
    printf("Average latency: %.2f us/op\n", (double)total_time / (NUM_BATCHES * BATCH_SIZE));
    
    /* Now demonstrate batch read operations */
    printf("\n=== Batch Read Verification ===\n");
    completed_ops = 0;
    failed_ops = 0;
    
    struct xpdk_batch_io read_ios[BATCH_SIZE];
    for (int i = 0; i < BATCH_SIZE; i++) {
        read_ios[i].type = XPDK_IO_READ;
        read_ios[i].fd = fd;
        read_ios[i].buffer = read_buffers[i];
        read_ios[i].count = BLOCK_SIZE;
        read_ios[i].offset = i * BLOCK_SIZE;  /* Read from first batch */
        read_ios[i].callback = batch_completion_callback;
        read_ios[i].ctx = &op_ids[i];
        read_ios[i].status = 0;
    }
    
    int rc = xpdk_batch_submit(batch_ctx, read_ios, BATCH_SIZE, NULL);
    if (rc == XPDK_SUCCESS) {
        /* Wait for read operations to complete */
        while ((completed_ops + failed_ops) < BATCH_SIZE) {
            xpdk_poll(10);
            usleep(100);
        }
        
        /* Verify data integrity */
        int verified = 0;
        for (int i = 0; i < BATCH_SIZE; i++) {
            char expected[BLOCK_SIZE];
            snprintf(expected, BLOCK_SIZE, "Batch write operation %d - test data", i);
            
            if (strncmp(read_buffers[i], expected, strlen(expected)) == 0) {
                verified++;
            } else {
                printf("Data mismatch in operation %d\n", i);
            }
        }
        
        printf("Data verification: %d/%d operations verified\n", verified, BATCH_SIZE);
    }
    
    /* Cleanup */
    xpdk_batch_cleanup(batch_ctx);
    
    for (int i = 0; i < BATCH_SIZE; i++) {
        xpdk_free_buffer(write_buffers[i]);
        xpdk_free_buffer(read_buffers[i]);
    }
    
    free(write_buffers);
    free(read_buffers);
    free(op_ids);
}

static void demonstrate_single_batch_submit(xpdk_fd_t fd)
{
    printf("\n=== Single Batch Submit Demonstration ===\n");
    
    struct xpdk_batch_ctx *ctx = xpdk_batch_init(32);
    if (!ctx) {
        printf("Failed to initialize batch context\n");
        return;
    }
    
    /* Submit individual operations one by one */
    const int NUM_OPS = 8;
    char *buffers[NUM_OPS];
    int op_ids[NUM_OPS];
    
    for (int i = 0; i < NUM_OPS; i++) {
        buffers[i] = xpdk_alloc_buffer(4096);
        snprintf(buffers[i], 4096, "Single submit operation %d", i);
        op_ids[i] = i + 100;  /* Different ID range */
        
        struct xpdk_batch_io io = {
            .type = XPDK_IO_WRITE,
            .fd = fd,
            .buffer = buffers[i],
            .count = 4096,
            .offset = (i + 1000) * 4096,  /* Different offset range */
            .callback = batch_completion_callback,
            .ctx = &op_ids[i],
            .status = 0
        };
        
        int rc = xpdk_batch_submit_one(ctx, &io);
        if (rc != XPDK_SUCCESS) {
            printf("Failed to submit operation %d: %s\n", i, xpdk_strerror(rc));
        } else {
            printf("Submitted operation %d\n", i);
        }
    }
    
    /* Wait for all operations to complete */
    completed_ops = 0;
    failed_ops = 0;
    
    while ((completed_ops + failed_ops) < NUM_OPS) {
        xpdk_poll(10);
        usleep(500);
    }
    
    printf("Single submit completed: %d success, %d failed\n", completed_ops, failed_ops);
    
    /* Cleanup */
    xpdk_batch_cleanup(ctx);
    for (int i = 0; i < NUM_OPS; i++) {
        xpdk_free_buffer(buffers[i]);
    }
}

static void demonstrate_performance_stats(xpdk_fd_t fd)
{
    printf("\n=== Performance Statistics Demonstration ===\n");
    
    /* Reset performance statistics */
    xpdk_reset_perf_stats(fd);
    
    /* Perform some I/O operations */
    char *buffer = xpdk_alloc_buffer(4096);
    strcpy(buffer, "Performance test data");
    
    uint64_t start_time = get_time_us();
    
    for (int i = 0; i < 100; i++) {
        xpdk_write(fd, buffer, 4096, i * 4096);
        if (i % 10 == 0) {
            xpdk_flush(fd);
        }
    }
    
    uint64_t end_time = get_time_us();
    
    /* Get performance statistics */
    struct xpdk_perf_stats stats;
    int rc = xpdk_get_perf_stats(fd, &stats);
    if (rc == XPDK_SUCCESS) {
        printf("Performance Statistics:\n");
        printf("  Total operations: %lu\n", stats.total_ops);
        printf("  Total bytes: %lu\n", stats.total_bytes);
        printf("  Read operations: %lu\n", stats.read_ops);
        printf("  Write operations: %lu\n", stats.write_ops);
        printf("  Average latency: %.2f us\n", stats.avg_latency_us);
        printf("  Current IOPS: %.2f\n", stats.current_iops);
        printf("  Current bandwidth: %.2f MB/s\n", stats.current_bps / (1024.0 * 1024.0));
        printf("  Error count: %lu\n", stats.error_count);
        printf("  Measured time: %.2f ms\n", (end_time - start_time) / 1000.0);
    } else {
        printf("Failed to get performance statistics: %s\n", xpdk_strerror(rc));
    }
    
    xpdk_free_buffer(buffer);
}

int main(int argc, char *argv[])
{
    const char *device_name = "Nvme0n1";
    
    if (argc > 1) {
        device_name = argv[1];
    }
    
    printf("=== XPDK Batch I/O Example ===\n");
    printf("Using device: %s\n", device_name);
    
    /* Initialize XPDK */
    struct xpdk_opts opts;
    xpdk_opts_init(&opts);
    
    int rc = xpdk_init_opts(&opts);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK: %s\n", xpdk_strerror(rc));
        return 1;
    }
    
    /* List available devices */
    struct xpdk_bdev_info devices[16];
    int num_devices = xpdk_list_bdevs(devices, 16);
    if (num_devices <= 0) {
        printf("No block devices found\n");
        xpdk_cleanup();
        return 1;
    }
    
    printf("Available devices:\n");
    for (int i = 0; i < num_devices; i++) {
        printf("  %s: %lu blocks x %lu bytes\n", 
               devices[i].name, devices[i].num_blocks, devices[i].block_size);
    }
    
    /* Open the device */
    xpdk_fd_t fd = xpdk_open(device_name, O_RDWR);
    if (fd < 0) {
        printf("Failed to open device %s: %s\n", device_name, xpdk_strerror(fd));
        xpdk_cleanup();
        return 1;
    }
    
    printf("Successfully opened device %s (fd=%d)\n", device_name, fd);
    
    /* Get device information */
    struct xpdk_bdev_info info;
    rc = xpdk_get_info(fd, &info);
    if (rc == XPDK_SUCCESS) {
        printf("Device info: %lu blocks, %lu bytes per block, %lu total bytes\n",
               info.num_blocks, info.block_size, info.num_blocks * info.block_size);
    }
    
    /* Demonstrate different batch I/O features */
    demonstrate_batch_io(fd);
    demonstrate_single_batch_submit(fd);
    demonstrate_performance_stats(fd);
    
    /* Close and cleanup */
    xpdk_close(fd);
    xpdk_cleanup();
    
    printf("\nBatch I/O example completed successfully!\n");
    return 0;
}
