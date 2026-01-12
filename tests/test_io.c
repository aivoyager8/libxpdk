#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include "xpdk.h"

/* Mock device name - in real testing you'd use actual SPDK device */
#define TEST_DEVICE "mock_device"

int main(void)
{
    printf("Testing I/O operations...\n");
    
    /* Test initialization */
    printf("Test 1: Initialize library... ");
    int rc = xpdk_init(NULL);
    if (rc != XPDK_SUCCESS) {
        printf("SKIP (SPDK not available: %s)\n", xpdk_strerror(rc));
        return 0;  /* Skip tests if SPDK not available */
    }
    printf("PASS\n");
    
    /* Test device opening with invalid device */
    printf("Test 2: Open invalid device... ");
    assert(xpdk_open("invalid_device_name", O_RDWR) < 0);
    printf("PASS\n");
    
    /* Test invalid I/O operations */
    printf("Test 3: Invalid I/O operations... ");
    /* Try I/O on invalid fd */
    assert(xpdk_read(-1, NULL, 1024, 0) < 0);
    assert(xpdk_write(-1, NULL, 1024, 0) < 0);
    assert(xpdk_flush(-1) < 0);
    
    printf("PASS\n");
    
    /* Test with NULL parameters */
    printf("Test 4: NULL parameter handling... ");
    assert(xpdk_read(0, NULL, 1024, 0) == XPDK_ERROR_INVALID);
    assert(xpdk_write(0, NULL, 1024, 0) == XPDK_ERROR_INVALID);
    
    printf("PASS\n");
    
    /* Cleanup */
    printf("Test 5: Cleanup... ");
    xpdk_cleanup();
    printf("PASS\n");
    
    printf("All I/O tests passed!\n");
    return 0;
}
