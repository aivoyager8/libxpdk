#include "xpdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <pthread.h>

/* Global performance tracking */
static volatile int total_completions = 0;
static volatile int total_errors = 0;
static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

/* Performance measurement helpers */
static uint64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

static void print_performance_summary(const char *test_name, int operations, 
                                     uint64_t time_us, size_t total_bytes)
{
    double ops_per_sec = (double)operations * 1000000.0 / time_us;
    double mbps = (double)total_bytes * 1000000.0 / (time_us * 1024 * 1024);
    double avg_latency = (double)time_us / operations;
    
    printf("\n=== %s Performance Summary ===\n", test_name);
    printf("Operations: %d\n", operations);
    printf("Total time: %.2f ms\n", time_us / 1000.0);
    printf("Throughput: %.2f ops/sec\n", ops_per_sec);
    printf("Bandwidth: %.2f MB/s\n", mbps);
    printf("Average latency: %.2f μs/op\n", avg_latency);
    printf("Errors: %d\n", total_errors);
}

/* Async callback for performance tests */
static void perf_callback(void *ctx, int status)
{
    if (status == XPDK_SUCCESS) {
        __sync_fetch_and_add(&total_completions, 1);
    } else {
        __sync_fetch_and_add(&total_errors, 1);
    }
}

/* Traditional synchronous I/O benchmark */
static void benchmark_sync_io(xpdk_fd_t fd, int num_ops, size_t block_size)
{
    printf("\n=== Synchronous I/O Benchmark ===\n");
    total_completions = 0;
    total_errors = 0;
    
    char *buffer = xpdk_alloc_buffer(block_size);
    memset(buffer, 0xAA, block_size);
    
    uint64_t start_time = get_time_us();
    
    for (int i = 0; i < num_ops; i++) {
        ssize_t result = xpdk_write(fd, buffer, block_size, i * block_size);
        if (result == (ssize_t)block_size) {
            total_completions++;
        } else {
            total_errors++;
        }
    }
    
    uint64_t end_time = get_time_us();
    
    print_performance_summary("Synchronous I/O", num_ops, 
                             end_time - start_time, num_ops * block_size);
    
    xpdk_free_buffer(buffer);
}

/* Asynchronous I/O benchmark */
static void benchmark_async_io(xpdk_fd_t fd, int num_ops, size_t block_size)
{
    printf("\n=== Asynchronous I/O Benchmark ===\n");
    total_completions = 0;
    total_errors = 0;
    
    char **buffers = malloc(num_ops * sizeof(char *));
    for (int i = 0; i < num_ops; i++) {
        buffers[i] = xpdk_alloc_buffer(block_size);
        memset(buffers[i], 0xBB, block_size);
    }
    
    uint64_t start_time = get_time_us();
    
    /* Submit all async operations */
    for (int i = 0; i < num_ops; i++) {
        int rc = xpdk_write_async(fd, buffers[i], block_size, 
                                 (i + num_ops) * block_size, perf_callback, NULL);
        if (rc != XPDK_SUCCESS) {
            total_errors++;
        }
    }
    
    /* Wait for all completions */
    while ((total_completions + total_errors) < num_ops) {
        xpdk_poll(50);
        usleep(100);
    }
    
    uint64_t end_time = get_time_us();
    
    print_performance_summary("Asynchronous I/O", num_ops, 
                             end_time - start_time, num_ops * block_size);
    
    for (int i = 0; i < num_ops; i++) {
        xpdk_free_buffer(buffers[i]);
    }
    free(buffers);
}

/* Vectored I/O benchmark */
static void benchmark_vectored_io(xpdk_fd_t fd, int num_ops, size_t block_size)
{
    printf("\n=== Vectored I/O Benchmark ===\n");
    total_completions = 0;
    total_errors = 0;
    
    const int vec_count = 4;  /* 4 buffers per vectored operation */
    int total_ops = num_ops / vec_count;
    
    uint64_t start_time = get_time_us();
    
    for (int i = 0; i < total_ops; i++) {
        /* Allocate buffers for this vectored operation */
        char *bufs[vec_count];
        struct xpdk_iovec iov[vec_count];
        
        for (int j = 0; j < vec_count; j++) {
            bufs[j] = xpdk_alloc_buffer(block_size);
            snprintf(bufs[j], block_size, "Vectored chunk %d-%d", i, j);
            iov[j].iov_base = bufs[j];
            iov[j].iov_len = block_size;
        }
        
        /* Perform vectored write */
        ssize_t result = xpdk_writev(fd, iov, vec_count, 
                                    (i + 2 * num_ops) * vec_count * block_size);
        
        if (result == (ssize_t)(vec_count * block_size)) {
            total_completions++;
        } else {
            total_errors++;
        }
        
        /* Cleanup */
        for (int j = 0; j < vec_count; j++) {
            xpdk_free_buffer(bufs[j]);
        }
    }
    
    uint64_t end_time = get_time_us();
    
    print_performance_summary("Vectored I/O", total_ops, 
                             end_time - start_time, total_ops * vec_count * block_size);
}

/* Batch I/O benchmark */
static void benchmark_batch_io(xpdk_fd_t fd, int num_ops, size_t block_size)
{
    printf("\n=== Batch I/O Benchmark ===\n");
    total_completions = 0;
    total_errors = 0;
    
    const int batch_size = 16;
    int num_batches = num_ops / batch_size;
    
    /* Initialize batch context */
    struct xpdk_batch_ctx *batch_ctx = xpdk_batch_init(batch_size);
    if (!batch_ctx) {
        printf("Failed to initialize batch context\n");
        return;
    }
    
    uint64_t start_time = get_time_us();
    
    for (int batch = 0; batch < num_batches; batch++) {
        /* Prepare batch operations */
        struct xpdk_batch_io ios[batch_size];
        char *buffers[batch_size];
        
        for (int i = 0; i < batch_size; i++) {
            buffers[i] = xpdk_alloc_buffer(block_size);
            snprintf(buffers[i], block_size, "Batch %d-%d", batch, i);
            
            ios[i].type = XPDK_IO_WRITE;
            ios[i].fd = fd;
            ios[i].buffer = buffers[i];
            ios[i].count = block_size;
            ios[i].offset = (batch * batch_size + i + 3 * num_ops) * block_size;
            ios[i].callback = perf_callback;
            ios[i].ctx = NULL;
            ios[i].status = 0;
        }
        
        /* Submit the batch */
        int rc = xpdk_batch_submit(batch_ctx, ios, batch_size, NULL);
        if (rc != XPDK_SUCCESS) {
            total_errors += batch_size;
        }
        
        /* Wait for this batch to complete */
        int batch_completed = 0;
        while (batch_completed < batch_size) {
            int prev_completions = total_completions + total_errors;
            xpdk_poll(10);
            usleep(100);
            int new_completions = total_completions + total_errors;
            batch_completed += (new_completions - prev_completions);
        }
        
        /* Cleanup buffers */
        for (int i = 0; i < batch_size; i++) {
            xpdk_free_buffer(buffers[i]);
        }
    }
    
    uint64_t end_time = get_time_us();
    
    print_performance_summary("Batch I/O", num_batches * batch_size, 
                             end_time - start_time, num_batches * batch_size * block_size);
    
    xpdk_batch_cleanup(batch_ctx);
}

/* QoS performance impact benchmark */
static void benchmark_qos_impact(xpdk_fd_t fd, int num_ops, size_t block_size)
{
    printf("\n=== QoS Impact Benchmark ===\n");
    
    /* Baseline performance without QoS */
    printf("Phase 1: Baseline (no QoS)\n");
    benchmark_sync_io(fd, num_ops / 2, block_size);
    
    /* Performance with QoS enabled */
    printf("Phase 2: With QoS (1000 IOPS limit)\n");
    if (xpdk_qos_enable_simple(fd, 1000, 0) == XPDK_SUCCESS) {
        benchmark_sync_io(fd, num_ops / 2, block_size);
        xpdk_qos_disable(fd);
    } else {
        printf("Failed to enable QoS\n");
    }
}

/* Comprehensive benchmark comparing all methods */
static void comprehensive_benchmark(xpdk_fd_t fd)
{
    const int NUM_OPS = 1000;
    const size_t BLOCK_SIZE = 4096;
    
    printf("\n=== Comprehensive Performance Benchmark ===\n");
    printf("Testing %d operations with %zu byte blocks\n", NUM_OPS, BLOCK_SIZE);
    
    /* Get initial device statistics */
    xpdk_reset_perf_stats(fd);
    
    /* Run all benchmark types */
    benchmark_sync_io(fd, NUM_OPS, BLOCK_SIZE);
    benchmark_async_io(fd, NUM_OPS, BLOCK_SIZE);
    benchmark_vectored_io(fd, NUM_OPS, BLOCK_SIZE);
    benchmark_batch_io(fd, NUM_OPS, BLOCK_SIZE);
    benchmark_qos_impact(fd, NUM_OPS, BLOCK_SIZE);
    
    /* Get final device statistics */
    struct xpdk_perf_stats final_stats;
    if (xpdk_get_perf_stats(fd, &final_stats) == XPDK_SUCCESS) {
        printf("\n=== Device Performance Statistics ===\n");
        printf("Total operations: %lu\n", final_stats.total_ops);
        printf("Total bytes: %lu MB\n", final_stats.total_bytes / (1024 * 1024));
        printf("Read operations: %lu\n", final_stats.read_ops);
        printf("Write operations: %lu\n", final_stats.write_ops);
        printf("Average latency: %.2f μs\n", final_stats.avg_latency_us);
        printf("Current IOPS: %.2f\n", final_stats.current_iops);
        printf("Current bandwidth: %.2f MB/s\n", final_stats.current_bps / (1024.0 * 1024.0));
        printf("Error count: %lu\n", final_stats.error_count);
    }
}

int main(int argc, char *argv[])
{
    const char *device_name = "Nvme0n1";
    bool turbo_mode = false;
    int cpu_core = -1;
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--turbo") == 0) {
            turbo_mode = true;
        } else if (strcmp(argv[i], "--cpu-core") == 0 && i + 1 < argc) {
            cpu_core = atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            device_name = argv[i];
        }
    }
    
    printf("=== XPDK Advanced Features Performance Benchmark ===\n");
    printf("Device: %s\n", device_name);
    printf("Turbo mode: %s\n", turbo_mode ? "enabled" : "disabled");
    if (cpu_core >= 0) {
        printf("CPU core: %d\n", cpu_core);
    }
    
    /* Initialize XPDK with appropriate options */
    struct xpdk_opts opts;
    xpdk_opts_init(&opts);
    opts.turbo_mode = turbo_mode;
    opts.cpu_core = cpu_core;
    if (turbo_mode) {
        opts.msg_ring_size = 2048;
        opts.msg_pool_size = 2048;
    }
    
    int rc = xpdk_init_opts(&opts);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK: %s\n", xpdk_strerror(rc));
        return 1;
    }
    
    /* Open the device */
    xpdk_fd_t fd = xpdk_open(device_name, O_RDWR);
    if (fd < 0) {
        printf("Failed to open device %s: %s\n", device_name, xpdk_strerror(fd));
        xpdk_cleanup();
        return 1;
    }
    
    /* Get device information */
    struct xpdk_bdev_info info;
    rc = xpdk_get_info(fd, &info);
    if (rc == XPDK_SUCCESS) {
        printf("Device info: %lu blocks x %lu bytes = %lu MB total\n",
               info.num_blocks, info.block_size, 
               (info.num_blocks * info.block_size) / (1024 * 1024));
    }
    
    /* Run comprehensive benchmark */
    comprehensive_benchmark(fd);
    
    /* Cleanup */
    xpdk_close(fd);
    xpdk_cleanup();
    
    printf("\n=== Benchmark Completed Successfully ===\n");
    return 0;
}
