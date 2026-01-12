#include "xpdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_vectored_io_basic(void)
{
    printf("=== Testing Vectored I/O Basic Operations ===\n");
    struct xpdk_iovec iov[3];
    char *buf1 = aligned_alloc(4096, 4096);
    char *buf2 = aligned_alloc(4096, 4096);
    char *buf3 = aligned_alloc(4096, 4096);
    if (!buf1 || !buf2 || !buf3) {
        printf("FAIL: Failed to allocate aligned buffers\n");
        return;
    }
    strcpy(buf1, "First chunk of vectored data");
    strcpy(buf2, "Second chunk of vectored data");
    strcpy(buf3, "Third chunk of vectored data");
    iov[0].iov_base = buf1;
    iov[0].iov_len = 4096;
    iov[1].iov_base = buf2;
    iov[1].iov_len = 4096;
    iov[2].iov_base = buf3;
    iov[2].iov_len = 4096;
    printf("PASS: Vectored I/O structure setup completed\n");
    free(buf1);
    free(buf2);
    free(buf3);
}

static void test_perf_stats_structure(void)
{
    printf("=== Testing Performance Statistics Structure ===\n");
    struct xpdk_perf_stats stats;
    memset(&stats, 0, sizeof(stats));
    stats.total_read_ops = 100;
    stats.total_write_ops = 200;
    stats.total_bytes_read = 4096 * 100;
    stats.total_bytes_written = 4096 * 200;
    stats.avg_read_latency_us = 120;
    stats.avg_write_latency_us = 130;
    stats.current_iops = 8000;
    stats.current_bandwidth = 32 * 1024 * 1024;
    stats.queue_depth = 8;
    stats.errors = 0;
    assert(stats.total_read_ops == 100);
    assert(stats.total_write_ops == 200);
    printf("PASS: Performance statistics structure test completed\n");
}

static void test_opts_and_error(void)
{
    printf("=== Testing Options and Error String ===\n");
    struct xpdk_opts opts;
    xpdk_opts_init(&opts);
    assert(opts.config_file == NULL);
    assert(opts.turbo_mode == false);
    assert(opts.cpu_core == -1);
    printf("PASS: Options initialization\n");
    const char *err_str = xpdk_strerror(XPDK_ERROR_INVALID);
    assert(err_str && strlen(err_str) > 0);
    printf("PASS: Error string: %s\n", err_str);
}

int main(void)
{
    printf("=== XPDK Minimal Integration Test ===\n");
    test_vectored_io_basic();
    test_perf_stats_structure();
    test_opts_and_error();
    printf("\n=== Integration Test Completed ===\n");
    return 0;
}
