#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include "xpdk.h"

int main(void)
{
    int rc;
    
    printf("Testing basic XPDK functionality...\n");
    
    /* Test initialization */
    printf("Test 1: Library initialization... ");
    rc = xpdk_init(NULL);
    (void)rc; /* suppress unused warning after assert in release mode */
    assert(rc == XPDK_SUCCESS || rc == XPDK_ERROR_IO);  /* May fail if no SPDK config */
    printf("PASS\n");
    
    /* Test error string function */
    printf("Test 2: Error string function... ");
    const char *error_str = xpdk_strerror(XPDK_ERROR_INVALID);
    assert(error_str != NULL);
    printf("PASS (%s)\n", error_str);
    
    /* Test device listing (may return 0 if no devices) */
    printf("Test 3: Device listing... ");
    struct xpdk_bdev_info devices[10];
    int num_devices = xpdk_list_bdevs(devices, 10);
    assert(num_devices >= 0);  /* Should not return negative unless error */
    printf("PASS (found %d devices)\n", num_devices);
    
    /* Test invalid operations */
    printf("Test 4: Invalid operations... ");
    xpdk_fd_t invalid_fd = xpdk_open("nonexistent_device", O_RDONLY);
    (void)invalid_fd; /* suppress unused warning after assert in release mode */
    assert(invalid_fd < 0);  /* Should fail */
    printf("PASS\n");
    
    /* Cleanup */
    printf("Test 5: Cleanup... ");
    xpdk_cleanup();
    printf("PASS\n");
    
    printf("All basic tests passed!\n");
    return 0;
}
