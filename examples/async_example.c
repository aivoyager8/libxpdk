#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "xpdk.h"

static volatile int io_completed = 0;
static int io_status = 0;

void async_callback(void *ctx, int status)
{
    const char *operation = (const char *)ctx;
    printf("Async %s completed with status: %s\n", 
           operation, xpdk_strerror(status));
    io_status = status;
    io_completed = 1;
}

int main(int argc, char *argv[])
{
    int rc;
    xpdk_fd_t fd;
    char write_data[4096];
    char read_data[4096];

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

    /* Prepare test data */
    memset(write_data, 0xBB, sizeof(write_data));
    snprintf(write_data, sizeof(write_data), 
             "Hello from XPDK async I/O! This is an asynchronous write operation.");

    printf("Starting async write operation...\n");
    io_completed = 0;
    rc = xpdk_write_async(fd, write_data, sizeof(write_data), 0, 
                          async_callback, (void *)"write");
    if (rc != XPDK_SUCCESS) {
        printf("Failed to start async write: %s\n", xpdk_strerror(rc));
        goto cleanup;
    }

    /* Poll for completion */
    printf("Polling for write completion...\n");
    while (!io_completed) {
        int completions = xpdk_poll(10);
        if (completions < 0) {
            printf("Poll failed: %s\n", xpdk_strerror(completions));
            break;
        }
        usleep(1000);  /* Sleep 1ms */
    }

    if (io_status != XPDK_SUCCESS) {
        printf("Async write failed\n");
        goto cleanup;
    }

    printf("Starting async read operation...\n");
    io_completed = 0;
    memset(read_data, 0, sizeof(read_data));
    rc = xpdk_read_async(fd, read_data, sizeof(read_data), 0, 
                         async_callback, (void *)"read");
    if (rc != XPDK_SUCCESS) {
        printf("Failed to start async read: %s\n", xpdk_strerror(rc));
        goto cleanup;
    }

    /* Poll for completion */
    printf("Polling for read completion...\n");
    while (!io_completed) {
        int completions = xpdk_poll(10);
        if (completions < 0) {
            printf("Poll failed: %s\n", xpdk_strerror(completions));
            break;
        }
        usleep(1000);  /* Sleep 1ms */
    }

    if (io_status == XPDK_SUCCESS) {
        /* Verify data */
        if (memcmp(write_data, read_data, sizeof(write_data)) == 0) {
            printf("Async I/O data verification successful!\n");
        } else {
            printf("Async I/O data verification failed!\n");
        }
    }

cleanup:
    printf("Closing device...\n");
    rc = xpdk_close(fd);
    if (rc != XPDK_SUCCESS) {
        printf("Failed to close device: %s\n", xpdk_strerror(rc));
    }

    printf("Cleaning up...\n");
    xpdk_cleanup();

    printf("Async example completed!\n");
    return 0;
}
