#include <stdio.h>
#include <stdlib.h>
#include "xpdk.h"

int main(void)
{
    int rc;
    struct xpdk_bdev_info devices[32];
    int num_devices;

    printf("Initializing XPDK library...\n");
    rc = xpdk_init(NULL);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK: %s\n", xpdk_strerror(rc));
        return 1;
    }

    printf("Listing available block devices...\n");
    num_devices = xpdk_list_bdevs(devices, sizeof(devices) / sizeof(devices[0]));
    if (num_devices < 0) {
        printf("Failed to list devices: %s\n", xpdk_strerror(num_devices));
        xpdk_cleanup();
        return 1;
    }

    printf("Found %d block device(s):\n", num_devices);
    printf("%-20s %-12s %-12s %-15s\n", "Device Name", "Block Size", "Num Blocks", "Capacity (MB)");
    printf("%-20s %-12s %-12s %-15s\n", "===========", "==========", "==========", "============");

    for (int i = 0; i < num_devices; i++) {
        double capacity_mb = (double)devices[i].capacity / (1024 * 1024);
        printf("%-20s %-12lu %-12lu %-15.2f\n",
               devices[i].name,
               devices[i].block_size,
               devices[i].num_blocks,
               capacity_mb);
    }

    if (num_devices == 0) {
        printf("No block devices found. Make sure SPDK is properly configured.\n");
    }

    printf("Cleaning up...\n");
    xpdk_cleanup();

    return 0;
}
