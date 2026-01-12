#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include "../include/xpdk.h"

#define TEST_BLOCK_SIZE 4096
#define TEST_BLOCKS 1000
#define PATTERN_MAGIC 0xDEADBEEF

static double get_time_diff(struct timeval *start, struct timeval *end)
{
    return (end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec) / 1000000.0;
}

static void fill_pattern(char *buffer, size_t size, uint32_t pattern, off_t offset)
{
    uint32_t *ptr = (uint32_t *)buffer;
    size_t count = size / sizeof(uint32_t);
    
    for (size_t i = 0; i < count; i++) {
        ptr[i] = pattern ^ (uint32_t)(offset + i);
    }
}

static int verify_pattern(char *buffer, size_t size, uint32_t pattern, off_t offset)
{
    uint32_t *ptr = (uint32_t *)buffer;
    size_t count = size / sizeof(uint32_t);
    
    for (size_t i = 0; i < count; i++) {
        uint32_t expected = pattern ^ (uint32_t)(offset + i);
        if (ptr[i] != expected) {
            printf("Pattern mismatch at offset %lu: expected 0x%x, got 0x%x\n",
                   offset + i * sizeof(uint32_t), expected, ptr[i]);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    struct xpdk_opts opts;
    struct xpdk_bdev_info devices[16];
    int device_count;
    xpdk_fd_t fd;
    char *write_buffer, *read_buffer;
    struct timeval start, end;
    double elapsed;
    int ret;
    int errors = 0;
    
    printf("=== libxpdk SPDK RAID1 测试程序 ===\n");
    
    /* 配置XPDK选项 */
    xpdk_opts_init(&opts);
    opts.turbo_mode = false;        /* 使用标准模式 */
    opts.cpu_core = -1;             /* 不绑定CPU */
    opts.config_file = NULL;        /* 不使用配置文件，使用默认配置 */
    
    printf("初始化libxpdk...\n");
    ret = xpdk_init_opts(&opts);
    if (ret != XPDK_SUCCESS) {
        printf("Failed to initialize libxpdk: %s\n", xpdk_strerror(ret));
        return 1;
    }
    
    printf("扫描可用设备...\n");
    device_count = xpdk_list_bdevs(devices, 16);
    if (device_count < 0) {
        printf("Failed to list devices: %s\n", xpdk_strerror(device_count));
        xpdk_cleanup();
        return 1;
    }
    
    printf("发现 %d 个设备:\n", device_count);
    for (int i = 0; i < device_count; i++) {
        printf("  [%d] %s - 大小: %lu MB, 块大小: %lu\n",
               i, devices[i].name,
               devices[i].capacity / (1024 * 1024),
               devices[i].block_size);
    }
    
    /* 寻找RAID1设备 */
    int raid_idx = -1;
    for (int i = 0; i < device_count; i++) {
        if (strstr(devices[i].name, "Raid1") != NULL) {
            raid_idx = i;
            break;
        }
    }
    
    if (raid_idx == -1) {
        printf("未找到RAID1设备!\n");
        xpdk_cleanup();
        return 1;
    }
    
    printf("\n使用RAID1设备: %s\n", devices[raid_idx].name);
    
    /* 打开RAID1设备 */
    fd = xpdk_open(devices[raid_idx].name, O_RDWR);
    if (fd < 0) {
        printf("Failed to open RAID1 device: %s\n", xpdk_strerror(fd));
        xpdk_cleanup();
        return 1;
    }
    
    printf("成功打开RAID1设备 (fd=%d)\n", fd);
    
    /* 分配测试缓冲区 */
    write_buffer = aligned_alloc(4096, TEST_BLOCK_SIZE);
    read_buffer = aligned_alloc(4096, TEST_BLOCK_SIZE);
    if (!write_buffer || !read_buffer) {
        printf("Failed to allocate test buffers\n");
        xpdk_close(fd);
        xpdk_cleanup();
        return 1;
    }
    
    printf("\n=== 开始RAID1 I/O测试 ===\n");
    
    /* 写入测试 */
    printf("1. 写入测试 (%d 个块，每块 %d 字节)...\n", TEST_BLOCKS, TEST_BLOCK_SIZE);
    gettimeofday(&start, NULL);
    
    for (int i = 0; i < TEST_BLOCKS; i++) {
        off_t offset = i * TEST_BLOCK_SIZE;
        fill_pattern(write_buffer, TEST_BLOCK_SIZE, PATTERN_MAGIC, offset);
        
        ssize_t written = xpdk_write(fd, write_buffer, TEST_BLOCK_SIZE, offset);
        if (written != TEST_BLOCK_SIZE) {
            printf("Write failed at block %d: %s\n", i, 
                   written < 0 ? xpdk_strerror(written) : "Short write");
            goto cleanup;
        }
        
        if ((i + 1) % 100 == 0) {
            printf("  已写入 %d/%d 块\n", i + 1, TEST_BLOCKS);
        }
    }
    
    gettimeofday(&end, NULL);
    elapsed = get_time_diff(&start, &end);
    printf("写入完成: %.2f MB/s (%.2f 秒)\n",
           (TEST_BLOCKS * TEST_BLOCK_SIZE / 1024.0 / 1024.0) / elapsed, elapsed);
    
    /* 同步数据 */
    printf("2. 同步数据到存储...\n");
    ret = xpdk_flush(fd);
    if (ret != XPDK_SUCCESS) {
        printf("Flush failed: %s\n", xpdk_strerror(ret));
    }
    
    /* 读取验证测试 */
    printf("3. 读取验证测试...\n");
    gettimeofday(&start, NULL);
    
    for (int i = 0; i < TEST_BLOCKS; i++) {
        off_t offset = i * TEST_BLOCK_SIZE;
        memset(read_buffer, 0, TEST_BLOCK_SIZE);
        
        ssize_t bytes_read = xpdk_read(fd, read_buffer, TEST_BLOCK_SIZE, offset);
        if (bytes_read != TEST_BLOCK_SIZE) {
            printf("Read failed at block %d: %s\n", i,
                   bytes_read < 0 ? xpdk_strerror(bytes_read) : "Short read");
            errors++;
            continue;
        }
        
        if (verify_pattern(read_buffer, TEST_BLOCK_SIZE, PATTERN_MAGIC, offset) != 0) {
            printf("Data verification failed at block %d\n", i);
            errors++;
        }
        
        if ((i + 1) % 100 == 0) {
            printf("  已验证 %d/%d 块\n", i + 1, TEST_BLOCKS);
        }
    }
    
    gettimeofday(&end, NULL);
    elapsed = get_time_diff(&start, &end);
    printf("读取完成: %.2f MB/s (%.2f 秒)\n",
           (TEST_BLOCKS * TEST_BLOCK_SIZE / 1024.0 / 1024.0) / elapsed, elapsed);
    
    if (errors == 0) {
        printf("\n✅ 所有数据验证通过! RAID1 工作正常!\n");
    } else {
        printf("\n❌ 发现 %d 个数据错误!\n", errors);
    }
    
    /* 获取设备信息 */
    printf("\n4. 设备信息:\n");
    struct xpdk_bdev_info info;
    ret = xpdk_get_info(fd, &info);
    if (ret == XPDK_SUCCESS) {
        printf("  设备名: %s\n", info.name);
        printf("  总大小: %lu MB\n", info.capacity / (1024 * 1024));
        printf("  块大小: %lu 字节\n", info.block_size);
    }
    
cleanup:
    free(write_buffer);
    free(read_buffer);
    xpdk_close(fd);
    xpdk_cleanup();
    
    printf("\n=== 测试完成 ===\n");
    return errors > 0 ? 1 : 0;
}
