#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "xpdk.h"

int main(int argc, char *argv[])
{
    int rc;
    xpdk_fd_t fd;
    char write_data[4096];
    char read_data[4096];
    struct xpdk_bdev_info info;

    if (argc != 2) {
        printf("Usage: %s <bdev_name>\n", argv[0]);
        return 1;
    }

    const char *bdev_name = argv[1];

    printf("Initializing XPDK library...\n");
    rc = xpdk_init(NULL);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK: %s\n", xpdk_strerror(rc));
        return 1;
    }

    printf("Opening device: %s\n", bdev_name);
    fd = xpdk_open(bdev_name, O_RDWR);
    if (fd < 0) {
        printf("Failed to open device %s: %s\n", bdev_name, xpdk_strerror(fd));
        xpdk_cleanup();
        return 1;
    }

    /* Get device information */
    rc = xpdk_get_info(fd, &info);
    if (rc == XPDK_SUCCESS) {
        printf("Device info:\n");
        printf("  Name: %s\n", info.name);
        printf("  Block size: %lu bytes\n", info.block_size);
        printf("  Number of blocks: %lu\n", info.num_blocks);
        printf("  Total capacity: %lu bytes (%.2f MB)\n", 
               info.capacity, (double)info.capacity / (1024 * 1024));
    }

    /* Prepare test data */
    memset(write_data, 0xAA, sizeof(write_data));
    snprintf(write_data, sizeof(write_data), 
             "Hello from XPDK! This is a test write operation.");

    printf("Writing test data...\n");
    ssize_t bytes_written = xpdk_write(fd, write_data, sizeof(write_data), 0);
    if (bytes_written < 0) {
        printf("Write failed: %s\n", xpdk_strerror((int)bytes_written));
    } else {
        printf("Successfully wrote %zd bytes\n", bytes_written);
    }

    /* Flush to ensure data is written */
    printf("Flushing data...\n");
    rc = xpdk_flush(fd);
    if (rc != XPDK_SUCCESS) {
        printf("Flush failed: %s\n", xpdk_strerror(rc));
    }

    /* Read back the data */
    printf("Reading data back...\n");
    memset(read_data, 0, sizeof(read_data));
    ssize_t bytes_read = xpdk_read(fd, read_data, sizeof(read_data), 0);
    if (bytes_read < 0) {
        printf("Read failed: %s\n", xpdk_strerror((int)bytes_read));
    } else {
        printf("Successfully read %zd bytes\n", bytes_read);
        
        /* Verify data */
        if (memcmp(write_data, read_data, sizeof(write_data)) == 0) {
            printf("Data verification successful!\n");
        } else {
            printf("Data verification failed!\n");
        }
    }

    printf("Closing device...\n");
    rc = xpdk_close(fd);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to close device: %s\n", xpdk_strerror(rc));
    }

    printf("Cleaning up...\n");
    xpdk_cleanup();

    printf("Example completed successfully!\n");
    return 0;
}
