#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include "xpdk.h"

static volatile int callback_called = 0;
static int callback_status = 0;

void test_callback(void *ctx, int status)
{
    int *user_data = (int *)ctx;
    *user_data = status;
    callback_status = status;
    callback_called = 1;
}

int main(void)
{
    printf("Testing async operations...\n");
    
    /* Test initialization */
    printf("Test 1: Initialize library... ");
    int rc = xpdk_init(NULL);
    if (rc != XPDK_SUCCESS) {
        printf("SKIP (SPDK not available: %s)\n", xpdk_strerror(rc));
        return 0;  /* Skip tests if SPDK not available */
    }
    printf("PASS\n");
    
    /* Test async operations with invalid fd */
    printf("Test 2: Async operations with invalid fd... ");
    char buffer[1024];
    int user_context = 0;
    
    rc = xpdk_read_async(-1, buffer, sizeof(buffer), 0, test_callback, &user_context);
    assert(rc == XPDK_ERROR_INVALID);
    
    rc = xpdk_write_async(-1, buffer, sizeof(buffer), 0, test_callback, &user_context);
    assert(rc == XPDK_ERROR_INVALID);
    
    printf("PASS\n");
    
    /* Test async operations with NULL callback */
    printf("Test 3: Async operations with NULL callback... ");
    rc = xpdk_read_async(0, buffer, sizeof(buffer), 0, NULL, &user_context);
    assert(rc == XPDK_ERROR_INVALID);
    
    rc = xpdk_write_async(0, buffer, sizeof(buffer), 0, NULL, &user_context);
    assert(rc == XPDK_ERROR_INVALID);
    
    printf("PASS\n");
    
    /* Test polling */
    printf("Test 4: Polling function... ");
    int poll_result = xpdk_poll(10);
    assert(poll_result >= 0);  /* Should not fail even if no I/O pending */
    printf("PASS\n");
    
    /* Cleanup */
    printf("Test 5: Cleanup... ");
    xpdk_cleanup();
    printf("PASS\n");
    
    printf("All async tests passed!\n");
    return 0;
}
