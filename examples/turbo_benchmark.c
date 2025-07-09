#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include "xpdk.h"

#define BENCHMARK_ITERATIONS 1000
#define BENCHMARK_BLOCK_SIZE 4096

/* Helper function to get current time in microseconds */
static uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

/* Benchmark function */
static void run_benchmark(const char *bdev_name, bool turbo_mode) {
    struct xpdk_opts opts;
    xpdk_fd_t fd;
    char *write_buffer, *read_buffer;
    uint64_t start_time, end_time;
    double elapsed_ms, iops, throughput_mb;
    int rc;
    
    printf("\n=== %s Mode Benchmark ===\n", turbo_mode ? "TURBO" : "STANDARD");
    
    /* Initialize XPDK with specified mode */
    xpdk_opts_init(&opts);
    opts.turbo_mode = turbo_mode;
    opts.cpu_core = turbo_mode ? 1 : -1;  /* Bind to CPU 1 in turbo mode */
    opts.msg_ring_size = turbo_mode ? 2048 : 1024;
    opts.msg_pool_size = turbo_mode ? 2048 : 1024;
    
    printf("Initializing XPDK...\n");
    rc = xpdk_init_opts(&opts);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK: %s\n", xpdk_strerror(rc));
        return;
    }
    
    /* Open device */
    printf("Opening device: %s\n", bdev_name);
    fd = xpdk_open(bdev_name, O_RDWR);
    if (fd < 0) {
        printf("Failed to open device: %s\n", xpdk_strerror(fd));
        xpdk_cleanup();
        return;
    }
    
    /* Allocate buffers */
    write_buffer = malloc(BENCHMARK_BLOCK_SIZE);
    read_buffer = malloc(BENCHMARK_BLOCK_SIZE);
    if (!write_buffer || !read_buffer) {
        printf("Failed to allocate buffers\n");
        goto cleanup;
    }
    
    /* Fill write buffer with test pattern */
    for (int i = 0; i < BENCHMARK_BLOCK_SIZE; i++) {
        write_buffer[i] = (char)(i % 256);
    }
    
    /* Write benchmark */
    printf("Running write benchmark (%d iterations)...\n", BENCHMARK_ITERATIONS);
    start_time = get_time_us();
    
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        uint64_t offset = (i % 100) * BENCHMARK_BLOCK_SIZE;  /* Rotate through 100 blocks */
        ssize_t bytes_written = xpdk_write(fd, write_buffer, BENCHMARK_BLOCK_SIZE, offset);
        if (bytes_written != BENCHMARK_BLOCK_SIZE) {
            printf("Write failed at iteration %d: %s\n", i, xpdk_strerror((int)bytes_written));
            goto cleanup;
        }
    }
    
    end_time = get_time_us();
    elapsed_ms = (end_time - start_time) / 1000.0;
    iops = (BENCHMARK_ITERATIONS * 1000.0) / elapsed_ms;
    throughput_mb = (BENCHMARK_ITERATIONS * BENCHMARK_BLOCK_SIZE * 1000.0) / (elapsed_ms * 1024 * 1024);
    
    printf("Write Results:\n");
    printf("  Time: %.2f ms\n", elapsed_ms);
    printf("  IOPS: %.2f\n", iops);
    printf("  Throughput: %.2f MB/s\n", throughput_mb);
    
    /* Read benchmark */
    printf("Running read benchmark (%d iterations)...\n", BENCHMARK_ITERATIONS);
    start_time = get_time_us();
    
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        uint64_t offset = (i % 100) * BENCHMARK_BLOCK_SIZE;  /* Read from same blocks */
        ssize_t bytes_read = xpdk_read(fd, read_buffer, BENCHMARK_BLOCK_SIZE, offset);
        if (bytes_read != BENCHMARK_BLOCK_SIZE) {
            printf("Read failed at iteration %d: %s\n", i, xpdk_strerror((int)bytes_read));
            goto cleanup;
        }
    }
    
    end_time = get_time_us();
    elapsed_ms = (end_time - start_time) / 1000.0;
    iops = (BENCHMARK_ITERATIONS * 1000.0) / elapsed_ms;
    throughput_mb = (BENCHMARK_ITERATIONS * BENCHMARK_BLOCK_SIZE * 1000.0) / (elapsed_ms * 1024 * 1024);
    
    printf("Read Results:\n");
    printf("  Time: %.2f ms\n", elapsed_ms);
    printf("  IOPS: %.2f\n", iops);
    printf("  Throughput: %.2f MB/s\n", throughput_mb);
    
    /* Verify data integrity */
    printf("Verifying data integrity...\n");
    ssize_t bytes_read = xpdk_read(fd, read_buffer, BENCHMARK_BLOCK_SIZE, 0);
    if (bytes_read == BENCHMARK_BLOCK_SIZE && memcmp(write_buffer, read_buffer, BENCHMARK_BLOCK_SIZE) == 0) {
        printf("Data integrity check: PASSED\n");
    } else {
        printf("Data integrity check: FAILED\n");
    }
    
cleanup:
    if (write_buffer) free(write_buffer);
    if (read_buffer) free(read_buffer);
    xpdk_close(fd);
    xpdk_cleanup();
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <bdev_name> [mode]\n", argv[0]);
        printf("  mode: 'turbo' for turbo mode, 'standard' for standard mode, 'both' for comparison\n");
        return 1;
    }
    
    const char *bdev_name = argv[1];
    const char *mode = argc > 2 ? argv[2] : "both";
    
    printf("XPDK Turbo Mode Benchmark\n");
    printf("Device: %s\n", bdev_name);
    printf("Block Size: %d bytes\n", BENCHMARK_BLOCK_SIZE);
    printf("Iterations: %d\n", BENCHMARK_ITERATIONS);
    
    if (strcmp(mode, "turbo") == 0) {
        run_benchmark(bdev_name, true);
    } else if (strcmp(mode, "standard") == 0) {
        run_benchmark(bdev_name, false);
    } else if (strcmp(mode, "both") == 0) {
        run_benchmark(bdev_name, false);  /* Standard mode first */
        sleep(1);  /* Brief pause between tests */
        run_benchmark(bdev_name, true);   /* Turbo mode second */
        
        printf("\n=== Performance Comparison ===\n");
        printf("Turbo mode typically provides:\n");
        printf("  - Lower latency (especially for small I/O)\n");
        printf("  - Higher CPU usage\n");
        printf("  - Better performance for latency-sensitive applications\n");
        printf("  - CPU core binding for consistent performance\n");
        printf("\nStandard mode provides:\n");
        printf("  - Lower CPU usage\n");
        printf("  - Better for throughput-oriented applications\n");
        printf("  - Suitable for multi-instance deployments\n");
        printf("  - No CPU core binding required\n");
    } else {
        printf("Invalid mode: %s\n", mode);
        return 1;
    }
    
    return 0;
}
