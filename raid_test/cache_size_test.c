#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <spdk/env.h>

int main()
{
    printf("=== SPDK cache_size计算测试 ===\n");
    
    /* 初始化SPDK环境 */
    struct spdk_env_opts env_opts;
    spdk_env_opts_init(&env_opts);
    env_opts.name = "cache_test";
    env_opts.shm_id = -1;
    
    int rc = spdk_env_init(&env_opts);
    if (rc < 0) {
        printf("SPDK环境初始化失败\n");
        return 1;
    }
    
    printf("SPDK环境初始化成功\n");
    
    /* 检查系统信息 */
    uint32_t lcore_count = spdk_env_get_core_count();
    printf("CPU核心数: %u\n", lcore_count);
    
    /* 测试不同pool_size和cache_size的组合 */
    struct test_case {
        size_t pool_size;
        size_t cache_size;
        const char *desc;
    } test_cases[] = {
        {32, 64, "原始参数(pool=32, cache=64)"},
        {32, 0, "无缓存(pool=32, cache=0)"},
        {128, 0, "无缓存(pool=128, cache=0)"},
        {128, 16, "合理缓存(pool=128, cache=16)"},
        {1024, 64, "大池(pool=1024, cache=64)"},
        {1024, 0, "大池无缓存(pool=1024, cache=0)"}
    };
    
    for (int i = 0; i < 6; i++) {
        size_t pool_size = test_cases[i].pool_size;
        size_t cache_size = test_cases[i].cache_size;
        
        /* 计算理论上的cache_size限制 */
        size_t max_cache = (pool_size / 2) / lcore_count;
        size_t actual_cache = (cache_size > max_cache) ? max_cache : cache_size;
        
        printf("\n测试 %s:\n", test_cases[i].desc);
        printf("  理论最大cache_size: %zu\n", max_cache);
        printf("  实际使用cache_size: %zu\n", actual_cache);
        
        char pool_name[32];
        snprintf(pool_name, sizeof(pool_name), "test_pool_%d", i);
        
        struct spdk_mempool *pool = spdk_mempool_create(pool_name, pool_size, 104, cache_size, SPDK_ENV_SOCKET_ID_ANY);
        if (pool != NULL) {
            printf("  结果: ✅ 成功\n");
            spdk_mempool_free(pool);
        } else {
            printf("  结果: ❌ 失败\n");
        }
    }
    
    return 0;
}
