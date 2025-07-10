#include <stdio.h>
#include <stdlib.h>
#include "../include/xpdk.h"
#include "../src/xpdk_internal.h"

int main()
{
    printf("=== libxpdk 内存调试程序 ===\n");
    printf("sizeof(struct xpdk_msg) = %zu 字节\n", sizeof(struct xpdk_msg));
    printf("sizeof(struct xpdk_opts) = %zu 字节\n", sizeof(struct xpdk_opts));
    printf("sizeof(struct xpdk_context) = %zu 字节\n", sizeof(struct xpdk_context));
    
    printf("\n测试SPDK环境初始化...\n");
    
    struct spdk_env_opts env_opts;
    spdk_env_opts_init(&env_opts);
    env_opts.name = "debug_test";
    env_opts.shm_id = -1;
    
    int rc = spdk_env_init(&env_opts);
    if (rc < 0) {
        printf("SPDK环境初始化失败\n");
        return 1;
    }
    printf("SPDK环境初始化成功\n");
    
    printf("\n测试内存池创建...\n");
    
    // 尝试创建不同大小的内存池
    for (int pool_size = 8; pool_size <= 128; pool_size *= 2) {
        struct spdk_mempool *pool = spdk_mempool_create("debug_pool", 
                                                        pool_size,
                                                        sizeof(struct xpdk_msg),
                                                        64,
                                                        SPDK_ENV_SOCKET_ID_ANY);
        if (pool != NULL) {
            printf("内存池创建成功: size=%d\n", pool_size);
            spdk_mempool_free(pool);
        } else {
            printf("内存池创建失败: size=%d\n", pool_size);
        }
    }
    
    printf("\n测试ring创建...\n");
    
    // 尝试创建不同大小的ring
    for (int ring_size = 8; ring_size <= 128; ring_size *= 2) {
        struct spdk_ring *ring = spdk_ring_create(SPDK_RING_TYPE_MP_SC, 
                                                  ring_size, 
                                                  SPDK_ENV_SOCKET_ID_ANY);
        if (ring != NULL) {
            printf("Ring创建成功: size=%d\n", ring_size);
            spdk_ring_free(ring);
        } else {
            printf("Ring创建失败: size=%d\n", ring_size);
        }
    }
    
    printf("调试完成\n");
    return 0;
}
