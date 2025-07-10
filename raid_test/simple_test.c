#include <stdio.h>
#include <stdlib.h>
#include "../include/xpdk.h"

int main(int argc, char *argv[])
{
    printf("=== Simple libxpdk Test ===\n");
    
    /* 使用最简单的初始化 */
    int ret = xpdk_init(NULL);
    if (ret != XPDK_SUCCESS) {
        printf("Failed to initialize libxpdk: %s\n", xpdk_strerror(ret));
        return 1;
    }
    
    printf("libxpdk initialized successfully!\n");
    
    xpdk_cleanup();
    printf("Test completed.\n");
    
    return 0;
}
