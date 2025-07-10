#include <stdio.h>
#include <stdlib.h>
#include "../include/xpdk.h"

int main()
{
    printf("=== libxpdk 最小初始化测试 ===\n");
    
    struct xpdk_opts opts;
    xpdk_opts_init(&opts);
    
    /* 使用最小参数 */
    opts.turbo_mode = false;
    opts.cpu_core = -1;
    opts.msg_ring_size = 8;    /* 最小ring */
    opts.msg_pool_size = 8;    /* 最小pool */
    opts.config_file = NULL;
    
    printf("正在初始化libxpdk（最小配置）...\n");
    int ret = xpdk_init_opts(&opts);
    if (ret != XPDK_SUCCESS) {
        printf("❌ 初始化失败: %s\n", xpdk_strerror(ret));
        return 1;
    }
    
    printf("✅ libxpdk初始化成功!\n");
    
    /* 快速清理并退出 */
    xpdk_cleanup();
    printf("✅ 清理完成\n");
    
    return 0;
}
