#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <spdk/env.h>

int main()
{
    printf("=== SPDK 基础内存测试 ===\n");
    
    /* 初始化SPDK环境 */
    struct spdk_env_opts env_opts;
    spdk_env_opts_init(&env_opts);
    env_opts.name = "basic_test";
    env_opts.shm_id = -1;
    // 尝试减少内存使用
    env_opts.mem_size = 128;  // 128MB
    
    printf("正在初始化SPDK环境 (内存大小: %d MB)...\n", env_opts.mem_size);
    int rc = spdk_env_init(&env_opts);
    if (rc < 0) {
        printf("SPDK环境初始化失败: %d\n", rc);
        return 1;
    }
    
    printf("SPDK环境初始化成功\n");
    
    /* 检查可用内存 */
    printf("检查SPDK内存池状态...\n");
    
    /* 尝试创建一个非常小的内存池 */
    printf("尝试创建最小内存池...\n");
    
    // 尝试创建一个只有1个元素，每个元素只有8字节的内存池
    struct spdk_mempool *tiny_pool = spdk_mempool_create("tiny", 1, 8, 0, SPDK_ENV_SOCKET_ID_ANY);
    if (tiny_pool != NULL) {
        printf("成功: 创建了8字节×1个元素的内存池\n");
        spdk_mempool_free(tiny_pool);
    } else {
        printf("失败: 连8字节×1个元素的内存池都无法创建\n");
    }
    
    // 尝试分配一块普通内存
    printf("尝试SPDK内存分配...\n");
    void *mem = spdk_malloc(1024, 64, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
    if (mem != NULL) {
        printf("成功: spdk_malloc分配了1024字节\n");
        spdk_free(mem);
    } else {
        printf("失败: spdk_malloc无法分配1024字节\n");
    }
    
    // 尝试分配hugepage内存
    void *huge_mem = spdk_zmalloc(4096, 4096, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
    if (huge_mem != NULL) {
        printf("成功: spdk_zmalloc分配了4096字节对齐内存\n");
        spdk_free(huge_mem);
    } else {
        printf("失败: spdk_zmalloc无法分配4096字节对齐内存\n");
    }
    
    printf("=== 测试完成 ===\n");
    return 0;
}
