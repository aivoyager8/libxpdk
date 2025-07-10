#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <spdk/env.h>
#include <rte_mempool.h>
#include <rte_errno.h>

int main()
{
    printf("=== DPDK mempool直接测试 ===\n");
    
    /* 初始化SPDK环境 */
    struct spdk_env_opts env_opts;
    spdk_env_opts_init(&env_opts);
    env_opts.name = "dpdk_test";
    env_opts.shm_id = -1;
    
    int rc = spdk_env_init(&env_opts);
    if (rc < 0) {
        printf("SPDK环境初始化失败: %d\n", rc);
        return 1;
    }
    
    printf("SPDK环境初始化成功\n");
    
    /* 直接使用DPDK的rte_mempool_create */
    printf("尝试直接使用DPDK的rte_mempool_create...\n");
    
    struct rte_mempool *mp = rte_mempool_create("direct_test", 32, 104, 0,
                                                0, NULL, NULL, NULL, NULL,
                                                SOCKET_ID_ANY, 0);
    if (mp != NULL) {
        printf("✅ DPDK rte_mempool_create成功!\n");
        rte_mempool_free(mp);
    } else {
        printf("❌ DPDK rte_mempool_create失败!\n");
        printf("DPDK错误码: %d\n", rte_errno);
        printf("错误信息: %s\n", rte_strerror(rte_errno));
    }
    
    /* 检查可用的内存 */
    printf("\n检查DPDK内存信息...\n");
    rte_dump_physmem_layout(stdout);
    
    return 0;
}
