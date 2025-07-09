#include "xpdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>

static volatile int batch_completions = 0;
static volatile int batch_errors = 0;

void batch_callback(void *ctx, int status)
{
    int *completion_count = (int *)ctx;
    if (status == XPDK_SUCCESS) {
        (*completion_count)++;
    } else {
        batch_errors++;
        printf("Batch I/O failed with status: %s\n", xpdk_strerror(status));
    }
    batch_completions++;
}

int main(int argc, char *argv[])
{
    struct xpdk_opts opts;
    struct xpdk_bdev_info devices[16];
    xpdk_fd_t fd;
    int rc, num_devices;
    
    printf("=== XPDK Batch I/O Test ===\n");
    
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
    
    /* Test 1: Basic batch I/O functionality */
    printf("\n=== Test 1: Basic Batch I/O ===\n");
    
    #define BATCH_SIZE 8
    #define BUFFER_SIZE 4096
    
    struct xpdk_batch_io batch_ios[BATCH_SIZE];
    char *write_buffers[BATCH_SIZE];
    char *read_buffers[BATCH_SIZE];
    int completion_counters[BATCH_SIZE];
    
    /* Allocate and prepare buffers */
    for (int i = 0; i < BATCH_SIZE; i++) {
        write_buffers[i] = xpdk_alloc_buffer(BUFFER_SIZE, 0);
        read_buffers[i] = xpdk_alloc_buffer(BUFFER_SIZE, 0);
        completion_counters[i] = 0;
        
        if (!write_buffers[i] || !read_buffers[i]) {
            printf("Failed to allocate buffer %d\n", i);
            goto cleanup_buffers;
        }
        
        /* Fill write buffer with pattern */
        memset(write_buffers[i], 'A' + i, BUFFER_SIZE);
    }
    
    /* Prepare batch write operations */
    for (int i = 0; i < BATCH_SIZE; i++) {
        batch_ios[i].type = XPDK_IO_WRITE;
        batch_ios[i].fd = fd;
        batch_ios[i].buffer = write_buffers[i];
        batch_ios[i].count = BUFFER_SIZE;
        batch_ios[i].offset = i * BUFFER_SIZE;
        batch_ios[i].user_ctx = &completion_counters[i];
        batch_ios[i].status = 0;
        batch_ios[i].bytes_transferred = 0;
    }
    
    printf("Submitting batch write operations (%d operations)...\n", BATCH_SIZE);
    batch_completions = 0;
    batch_errors = 0;
    
    rc = xpdk_batch_submit(batch_ios, BATCH_SIZE, batch_callback);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to submit batch write: %s\n", xpdk_strerror(rc));
        goto cleanup_buffers;
    }
    
    /* Wait for all writes to complete */
    printf("Waiting for batch write completions...\n");
    int wait_count = 0;
    while (batch_completions < BATCH_SIZE && wait_count < 100) {
        int completions = xpdk_batch_wait(BATCH_SIZE, 100); /* 100ms timeout */
        if (completions < 0) {
            printf("Batch wait failed: %s\n", xpdk_strerror(completions));
            break;
        }
        wait_count++;
        usleep(10000); /* 10ms */
    }
    
    if (batch_completions == BATCH_SIZE && batch_errors == 0) {
        printf("PASS: Batch write completed successfully (%d operations)\n", batch_completions);
    } else {
        printf("FAIL: Batch write - completed: %d, errors: %d\n", batch_completions, batch_errors);
    }
    
    /* Test 2: Batch read operations */
    printf("\n=== Test 2: Batch Read Operations ===\n");
    
    /* Prepare batch read operations */
    for (int i = 0; i < BATCH_SIZE; i++) {
        batch_ios[i].type = XPDK_IO_READ;
        batch_ios[i].fd = fd;
        batch_ios[i].buffer = read_buffers[i];
        batch_ios[i].count = BUFFER_SIZE;
        batch_ios[i].offset = i * BUFFER_SIZE;
        batch_ios[i].user_ctx = &completion_counters[i];
        batch_ios[i].status = 0;
        batch_ios[i].bytes_transferred = 0;
        completion_counters[i] = 0;
    }
    
    printf("Submitting batch read operations (%d operations)...\n", BATCH_SIZE);
    batch_completions = 0;
    batch_errors = 0;
    
    rc = xpdk_batch_submit(batch_ios, BATCH_SIZE, batch_callback);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to submit batch read: %s\n", xpdk_strerror(rc));
        goto cleanup_buffers;
    }
    
    /* Wait for all reads to complete */
    printf("Waiting for batch read completions...\n");
    wait_count = 0;
    while (batch_completions < BATCH_SIZE && wait_count < 100) {
        int completions = xpdk_batch_wait(BATCH_SIZE, 100);
        if (completions < 0) {
            printf("Batch wait failed: %s\n", xpdk_strerror(completions));
            break;
        }
        wait_count++;
        usleep(10000);
    }
    
    if (batch_completions == BATCH_SIZE && batch_errors == 0) {
        printf("PASS: Batch read completed successfully (%d operations)\n", batch_completions);
        
        /* Verify data integrity */
        bool data_ok = true;
        for (int i = 0; i < BATCH_SIZE; i++) {
            char expected_pattern = 'A' + i;
            for (int j = 0; j < BUFFER_SIZE; j++) {
                if (read_buffers[i][j] != expected_pattern) {
                    printf("Data mismatch in buffer %d at position %d: expected %c, got %c\n",
                           i, j, expected_pattern, read_buffers[i][j]);
                    data_ok = false;
                    break;
                }
            }
            if (!data_ok) break;
        }
        
        if (data_ok) {
            printf("PASS: Data integrity check\n");
        } else {
            printf("FAIL: Data integrity check\n");
        }
    } else {
        printf("FAIL: Batch read - completed: %d, errors: %d\n", batch_completions, batch_errors);
    }
    
    /* Test 3: Batch context interface */
    printf("\n=== Test 3: Batch Context Interface ===\n");
    
    struct xpdk_batch_ctx *batch_ctx = xpdk_batch_init(16);
    if (!batch_ctx) {
        printf("FAIL: Failed to initialize batch context\n");
        goto cleanup_buffers;
    }
    
    printf("PASS: Batch context initialized\n");
    
    /* Submit individual I/O operations to batch context */
    int submitted = 0;
    for (int i = 0; i < 4; i++) {
        struct xpdk_batch_io io = {
            .type = XPDK_IO_WRITE,
            .fd = fd,
            .buffer = write_buffers[i],
            .count = BUFFER_SIZE,
            .offset = i * BUFFER_SIZE,
            .user_ctx = &completion_counters[i]
        };
        
        rc = xpdk_batch_submit_one(batch_ctx, &io);
        if (rc == XPDK_SUCCESS) {
            submitted++;
        } else if (rc == XPDK_ERROR_BUSY) {
            printf("Batch queue full after %d submissions\n", submitted);
            break;
        } else {
            printf("Batch submit failed: %s\n", xpdk_strerror(rc));
            break;
        }
    }
    
    printf("Submitted %d operations to batch context\n", submitted);
    
    /* Process completions */
    sleep(1); /* Allow some time for completions */
    int processed = xpdk_batch_process_completions(batch_ctx, 10);
    printf("Processed %d completions from batch context\n", processed);
    
    xpdk_batch_cleanup(batch_ctx);
    printf("PASS: Batch context cleanup completed\n");
    
cleanup_buffers:
    /* Free allocated buffers */
    for (int i = 0; i < BATCH_SIZE; i++) {
        if (write_buffers[i]) xpdk_free_buffer(write_buffers[i]);
        if (read_buffers[i]) xpdk_free_buffer(read_buffers[i]);
    }
    
    /* Close device */
    rc = xpdk_close(fd);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to close device: %s\n", xpdk_strerror(rc));
    }
    
    /* Cleanup */
    xpdk_cleanup();
    
    printf("\nBatch I/O test completed\n");
    return 0;
}
