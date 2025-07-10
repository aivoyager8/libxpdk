#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <spdk/env.h>

int main()
{
    printf("=== SPDK mempool详细测试 ===\n");
    
    /* 初始化SPDK环境 */
    struct spdk_env_opts env_opts;
    spdk_env_opts_init(&env_opts);
    env_opts.name = "mempool_test";
    env_opts.shm_id = -1;
    
    int rc = spdk_env_init(&env_opts);
    if (rc < 0) {
        printf("SPDK环境初始化失败\n");
        return 1;
    }
    
    printf("SPDK环境初始化成功\n");
    
    /* 测试不同的cache_size */
    printf("\n测试不同的cache_size值:\n");
    for (int cache_size = 0; cache_size <= 256; cache_size += 32) {
        struct spdk_mempool *pool = spdk_mempool_create("test_pool", 32, 104, cache_size, SPDK_ENV_SOCKET_ID_ANY);
        if (pool != NULL) {
            printf("cache_size=%d: 成功\n", cache_size);
            spdk_mempool_free(pool);
            break;  // 找到第一个可工作的cache_size就停止
        } else {
            printf("cache_size=%d: 失败\n", cache_size);
        }
    }
    
    /* 测试不同的element_size */
    printf("\n测试不同的element_size值:\n");
    for (int element_size = 64; element_size <= 256; element_size += 16) {
        struct spdk_mempool *pool = spdk_mempool_create("test_pool2", 32, element_size, 0, SPDK_ENV_SOCKET_ID_ANY);
        if (pool != NULL) {
            printf("element_size=%d: 成功\n", element_size);
            spdk_mempool_free(pool);
        } else {
            printf("element_size=%d: 失败\n", element_size);
        }
    }
    
    /* 测试较小的pool_size */
    printf("\n测试不同的pool_size值:\n");
    for (int pool_size = 1; pool_size <= 64; pool_size *= 2) {
        struct spdk_mempool *pool = spdk_mempool_create("test_pool3", pool_size, 104, 0, SPDK_ENV_SOCKET_ID_ANY);
        if (pool != NULL) {
            printf("pool_size=%d: 成功\n", pool_size);
            spdk_mempool_free(pool);
        } else {
            printf("pool_size=%d: 失败\n", pool_size);
        }
    }
    
    /* 测试原始参数但使用cache_size=0 */
    printf("\n测试原始参数但cache_size=0:\n");
    struct spdk_mempool *pool = spdk_mempool_create("xpdk_msg_pool", 32, 104, 0, SPDK_ENV_SOCKET_ID_ANY);
    if (pool != NULL) {
        printf("成功创建内存池 (cache_size=0)\n");
        spdk_mempool_free(pool);
    } else {
        printf("仍然失败 (cache_size=0)\n");
    }
    
    return 0;
}
