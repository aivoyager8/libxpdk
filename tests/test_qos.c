#include <fcntl.h>
#include "xpdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

int main(void)
{
    struct xpdk_opts opts;
    struct xpdk_bdev_info devices[16];
    xpdk_fd_t fd = -1;
    uint64_t qos_limits[XPDK_QOS_NUM_RATE_LIMIT_TYPES];
    char buffer[4096];
    int rc = 0, num_devices = 0;

    printf("=== XPDK QoS Test ===\n");

    /* 主线程初始化 XPDK，只允许一次 */
    xpdk_opts_init(&opts);
    opts.turbo_mode = true; // 推荐高性能模式
    opts.cpu_core = -1;     // 默认不绑定 CPU
    opts.msg_ring_size = 1024; // 推荐默认
    opts.poll_period_us = 0;   // busy polling
    rc = xpdk_init_opts(&opts);
    if (rc != XPDK_SUCCESS) {
        fprintf(stderr, "[TEST-ERROR] Failed to initialize XPDK: %s\n", xpdk_strerror(rc));
        return 1;
    }

    /* 列出可用设备 */
    num_devices = xpdk_list_bdevs(devices, 16);
    if (num_devices <= 0) {
        fprintf(stderr, "[TEST-ERROR] No block devices found\n");
        xpdk_cleanup();
        return 1;
    }
    printf("Found %d devices, using: %s\n", num_devices, devices[0].name);

    /* 打开第一个设备 */
    fd = xpdk_open(devices[0].name, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "[TEST-ERROR] Failed to open device: %s\n", xpdk_strerror(fd));
        xpdk_cleanup();
        return 1;
    }

    /* Test 1: Get initial QoS limits (should be all zeros) */
    printf("\n=== Test 1: Initial QoS limits ===\n");
    rc = xpdk_qos_get_rate_limits(fd, qos_limits);
    if (rc == XPDK_SUCCESS) {
        printf("Initial QoS limits:\n");
        printf("  RW IOPS: %lu\n", qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT]);
        printf("  RW BPS:  %lu\n", qos_limits[XPDK_QOS_RW_BPS_RATE_LIMIT]);
        printf("  R BPS:   %lu\n", qos_limits[XPDK_QOS_R_BPS_RATE_LIMIT]);
        printf("  W BPS:   %lu\n", qos_limits[XPDK_QOS_W_BPS_RATE_LIMIT]);
        printf("PASS: Get initial limits\n");
    } else {
        printf("FAIL: Get initial limits - %s\n", xpdk_strerror(rc));
    }

    /* Test 2: Set QoS limits using simple interface */
    printf("\n=== Test 2: Set simple QoS limits ===\n");
    rc = xpdk_qos_enable_simple(fd, 1000, 4 * 1024 * 1024);
    if (rc == XPDK_SUCCESS) {
        printf("PASS: Set simple QoS limits\n");

        /* Verify the limits were set */
        rc = xpdk_qos_get_rate_limits(fd, qos_limits);
        if (rc == XPDK_SUCCESS) {
            printf("Verified QoS limits:\n");
            printf("  RW IOPS: %lu (expected: 1000)\n", qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT]);
            printf("  RW BPS:  %lu (expected: %lu)\n", qos_limits[XPDK_QOS_RW_BPS_RATE_LIMIT], 4UL * 1024 * 1024);

            if (qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT] == 1000 &&
                qos_limits[XPDK_QOS_RW_BPS_RATE_LIMIT] == 4UL * 1024 * 1024) {
                printf("PASS: Verify simple QoS limits\n");
            } else {
                printf("FAIL: Verify simple QoS limits - values don't match\n");
            }
        } else {
            printf("FAIL: Verify simple QoS limits - %s\n", xpdk_strerror(rc));
        }
    } else {
        printf("FAIL: Set simple QoS limits - %s\n", xpdk_strerror(rc));
    }

    /* Test 3: Set advanced QoS limits */
    printf("\n=== Test 3: Set advanced QoS limits ===\n");
    memset(qos_limits, 0, sizeof(qos_limits));
    qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT] = 500;
    qos_limits[XPDK_QOS_R_BPS_RATE_LIMIT] = 2 * 1024 * 1024;
    qos_limits[XPDK_QOS_W_BPS_RATE_LIMIT] = 1 * 1024 * 1024;

    rc = xpdk_qos_set_rate_limits(fd, qos_limits);
    if (rc == XPDK_SUCCESS) {
        printf("PASS: Set advanced QoS limits\n");

        /* Verify the limits were set */
        memset(qos_limits, 0, sizeof(qos_limits));
        rc = xpdk_qos_get_rate_limits(fd, qos_limits);
        if (rc == XPDK_SUCCESS) {
            printf("Verified advanced QoS limits:\n");
            printf("  RW IOPS: %lu (expected: 500)\n", qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT]);
            printf("  RW BPS:  %lu (expected: 0)\n", qos_limits[XPDK_QOS_RW_BPS_RATE_LIMIT]);
            printf("  R BPS:   %lu (expected: %lu)\n", qos_limits[XPDK_QOS_R_BPS_RATE_LIMIT], 2UL * 1024 * 1024);
            printf("  W BPS:   %lu (expected: %lu)\n", qos_limits[XPDK_QOS_W_BPS_RATE_LIMIT], 1UL * 1024 * 1024);

            if (qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT] == 500 &&
                qos_limits[XPDK_QOS_RW_BPS_RATE_LIMIT] == 0 &&
                qos_limits[XPDK_QOS_R_BPS_RATE_LIMIT] == 2UL * 1024 * 1024 &&
                qos_limits[XPDK_QOS_W_BPS_RATE_LIMIT] == 1UL * 1024 * 1024) {
                printf("PASS: Verify advanced QoS limits\n");
            } else {
                printf("FAIL: Verify advanced QoS limits - values don't match\n");
            }
        } else {
            printf("FAIL: Verify advanced QoS limits - %s\n", xpdk_strerror(rc));
        }
    } else {
        printf("FAIL: Set advanced QoS limits - %s\n", xpdk_strerror(rc));
    }

    /* Test 4: Perform I/O operations with QoS */
    printf("\n=== Test 4: I/O operations with QoS ===\n");
    memset(buffer, 0xAB, sizeof(buffer));
    ssize_t bytes_written = xpdk_write(fd, buffer, sizeof(buffer), 0);
    if (bytes_written == sizeof(buffer)) {
        printf("PASS: Write with QoS - %zd bytes\n", bytes_written);
    } else {
        printf("FAIL: Write with QoS - %s\n", xpdk_strerror((int)bytes_written));
    }

    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = xpdk_read(fd, buffer, sizeof(buffer), 0);
    if (bytes_read == sizeof(buffer)) {
        printf("PASS: Read with QoS - %zd bytes\n", bytes_read);
    } else {
        printf("FAIL: Read with QoS - %s\n", xpdk_strerror((int)bytes_read));
    }

    /* Test 5: Disable QoS */
    printf("\n=== Test 5: Disable QoS ===\n");
    rc = xpdk_qos_disable(fd);
    if (rc == XPDK_SUCCESS) {
        printf("PASS: Disable QoS\n");

        /* Verify QoS is disabled */
        rc = xpdk_qos_get_rate_limits(fd, qos_limits);
        if (rc == XPDK_SUCCESS) {
            printf("QoS limits after disable:\n");
            printf("  RW IOPS: %lu\n", qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT]);
            printf("  RW BPS:  %lu\n", qos_limits[XPDK_QOS_RW_BPS_RATE_LIMIT]);
            printf("  R BPS:   %lu\n", qos_limits[XPDK_QOS_R_BPS_RATE_LIMIT]);
            printf("  W BPS:   %lu\n", qos_limits[XPDK_QOS_W_BPS_RATE_LIMIT]);

            if (qos_limits[XPDK_QOS_RW_IOPS_RATE_LIMIT] == 0 &&
                qos_limits[XPDK_QOS_RW_BPS_RATE_LIMIT] == 0 &&
                qos_limits[XPDK_QOS_R_BPS_RATE_LIMIT] == 0 &&
                qos_limits[XPDK_QOS_W_BPS_RATE_LIMIT] == 0) {
                printf("PASS: Verify QoS disabled\n");
            } else {
                printf("FAIL: Verify QoS disabled - values not zero\n");
            }
        } else {
            printf("FAIL: Verify QoS disabled - %s\n", xpdk_strerror(rc));
        }
    } else {
        printf("FAIL: Disable QoS - %s\n", xpdk_strerror(rc));
    }

    /* Test 6: Test QoS type names */
    printf("\n=== Test 6: QoS type names ===\n");
    for (int i = 0; i < XPDK_QOS_NUM_RATE_LIMIT_TYPES; i++) {
        const char *name = xpdk_qos_get_rate_limit_name(i);
        printf("  Type %d: %s\n", i, name);
    }
    printf("PASS: QoS type names\n");

    /* Cleanup */
    rc = xpdk_close(fd);
    if (rc != XPDK_SUCCESS) {
        printf("FAIL: Close device - %s\n", xpdk_strerror(rc));
    }

    xpdk_cleanup();

    printf("\n=== QoS Test Complete ===\n");
    return 0;
}
