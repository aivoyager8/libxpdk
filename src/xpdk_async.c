#include "xpdk_internal.h"
#include <stdlib.h>
#include <spdk/bdev.h>

/* Asynchronous I/O operations using the high-performance message system */

int
xpdk_read_async(xpdk_fd_t fd, void *buffer, size_t count, uint64_t offset,
                xpdk_io_callback_t callback, void *ctx)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (buffer == NULL || count == 0 || callback == NULL) {
        return XPDK_ERROR_INVALID;
    }

    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_READ);
    if (msg == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    msg->io.fd = fd;
    msg->io.buffer = buffer;
    msg->io.count = count;
    msg->io.offset = offset;
    msg->io.callback = callback;    /* Mark as async */
    msg->io.user_ctx = ctx;

    /* Send async message - no waiting */
    int rc = xpdk_msg_send_async(msg);
    if (rc != XPDK_SUCCESS) {
        xpdk_msg_free(msg);
        return rc;
    }

    /* Message will be freed by completion callback */
    return XPDK_SUCCESS;
}

int
xpdk_write_async(xpdk_fd_t fd, const void *buffer, size_t count, uint64_t offset,
                 xpdk_io_callback_t callback, void *ctx)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (buffer == NULL || count == 0 || callback == NULL) {
        return XPDK_ERROR_INVALID;
    }

    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_WRITE);
    if (msg == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    msg->io.fd = fd;
    msg->io.buffer = (void *)buffer; /* Cast away const for unified interface */
    msg->io.count = count;
    msg->io.offset = offset;
    msg->io.callback = callback;     /* Mark as async */
    msg->io.user_ctx = ctx;

    /* Send async message - no waiting */
    int rc = xpdk_msg_send_async(msg);
    if (rc != XPDK_SUCCESS) {
        xpdk_msg_free(msg);
        return rc;
    }

    /* Message will be freed by completion callback */
    return XPDK_SUCCESS;
}

int
xpdk_poll(int max_completions)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (max_completions <= 0) {
        max_completions = 1;
    }

    /* 
     * Poll the SPDK thread for I/O completions
     * This is a lightweight operation that processes pending events
     * The actual polling happens in the SPDK thread's event loop
     */
    return spdk_thread_poll(g_xpdk_ctx.spdk_thread, max_completions, 0);
}
