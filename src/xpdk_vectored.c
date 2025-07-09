#include "xpdk_internal.h"
#include <string.h>
#include <stdlib.h>

/* Context for vectored I/O operations */
struct xpdk_vectored_ctx {
    struct xpdk_iovec *iov;
    int iovcnt;
    void *temp_buffer;
    bool is_read;
    xpdk_completion_callback_t original_callback;
    void *original_ctx;
};

/* Forward declarations */
static void xpdk_vectored_completion_callback(void *ctx, int status);

/* Vectored I/O operations implementation */

/* Helper function to calculate total size of iovec array */
static size_t calculate_iovec_size(const struct xpdk_iovec *iov, int iovcnt)
{
    size_t total_size = 0;
    for (int i = 0; i < iovcnt; i++) {
        total_size += iov[i].iov_len;
    }
    return total_size;
}

/* Helper function to copy data from iovec to linear buffer */
static void iovec_to_buffer(const struct xpdk_iovec *iov, int iovcnt, void *buffer)
{
    char *dst = (char *)buffer;
    for (int i = 0; i < iovcnt; i++) {
        memcpy(dst, iov[i].iov_base, iov[i].iov_len);
        dst += iov[i].iov_len;
    }
}

/* Helper function to copy data from linear buffer to iovec */
static void buffer_to_iovec(const void *buffer, const struct xpdk_iovec *iov, int iovcnt)
{
    const char *src = (const char *)buffer;
    for (int i = 0; i < iovcnt; i++) {
        memcpy(iov[i].iov_base, src, iov[i].iov_len);
        src += iov[i].iov_len;
    }
}

/* High-performance vectored read operation (synchronous) */
ssize_t xpdk_readv(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (iov == NULL || iovcnt <= 0 || iovcnt > 32) {
        return XPDK_ERROR_INVALID;
    }

    /* Send native vectored read message to SPDK thread */
    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_READV_NATIVE);
    if (msg == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    msg->io.fd = fd;
    msg->io.buffer = (void *)iov;  /* Pass iov directly */
    msg->io.count = iovcnt;
    msg->io.offset = offset;
    msg->io.callback = NULL;  /* Synchronous */
    msg->io.user_ctx = NULL;

    int rc = xpdk_msg_send_sync(msg);
    if (rc != XPDK_SUCCESS) {
        xpdk_msg_free(msg);
        return rc;
    }

    ssize_t bytes_read = msg->io.bytes_transferred;
    xpdk_msg_free(msg);
    return bytes_read;
}

/* High-performance vectored write operation (synchronous) */
ssize_t xpdk_writev(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (iov == NULL || iovcnt <= 0 || iovcnt > 32) {
        return XPDK_ERROR_INVALID;
    }

    /* Send native vectored write message to SPDK thread */
    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_WRITEV_NATIVE);
    if (msg == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    msg->io.fd = fd;
    msg->io.buffer = (void *)iov;  /* Pass iov directly */
    msg->io.count = iovcnt;
    msg->io.offset = offset;
    msg->io.callback = NULL;  /* Synchronous */
    msg->io.user_ctx = NULL;

    int rc = xpdk_msg_send_sync(msg);
    if (rc != XPDK_SUCCESS) {
        xpdk_msg_free(msg);
        return rc;
    }

    ssize_t bytes_written = msg->io.bytes_transferred;
    xpdk_msg_free(msg);
    return bytes_written;
}

/* High-performance asynchronous vectored read operation */
int xpdk_readv_async(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset,
                     xpdk_io_callback_t callback, void *ctx)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (iov == NULL || iovcnt <= 0 || iovcnt > 32 || callback == NULL) {
        return XPDK_ERROR_INVALID;
    }

    /* Send native vectored read message to SPDK thread */
    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_READV_NATIVE);
    if (msg == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    msg->io.fd = fd;
    msg->io.buffer = (void *)iov;  /* Pass iov directly */
    msg->io.count = iovcnt;
    msg->io.offset = offset;
    msg->io.callback = callback;  /* Asynchronous */
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

/* High-performance asynchronous vectored write operation */
int xpdk_writev_async(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset,
                      xpdk_io_callback_t callback, void *ctx)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (iov == NULL || iovcnt <= 0 || iovcnt > 32 || callback == NULL) {
        return XPDK_ERROR_INVALID;
    }

    /* Send native vectored write message to SPDK thread */
    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_WRITEV_NATIVE);
    if (msg == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    msg->io.fd = fd;
    msg->io.buffer = (void *)iov;  /* Pass iov directly */
    msg->io.count = iovcnt;
    msg->io.offset = offset;
    msg->io.callback = callback;  /* Asynchronous */
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

/* Completion callback for vectored operations */
static void xpdk_vectored_completion_callback(void *ctx, int status)
{
    struct xpdk_vectored_ctx *v_ctx = (struct xpdk_vectored_ctx *)ctx;
    
    /* For read operations, copy data back to original iovec */
    if (v_ctx->is_read && status == XPDK_SUCCESS) {
        buffer_to_iovec(v_ctx->temp_buffer, v_ctx->iov, v_ctx->iovcnt);
    }

    /* Call original callback */
    v_ctx->original_callback(v_ctx->original_ctx, status);

    /* Cleanup */
    xpdk_free_buffer(v_ctx->temp_buffer);
    free(v_ctx);
}

/* Buffer allocation functions */
void *xpdk_alloc_buffer(size_t size, size_t alignment)
{
    if (size == 0) {
        return NULL;
    }

    /* Use default alignment if not specified */
    if (alignment == 0) {
        alignment = 4096; /* 4KB default alignment for most SSDs */
    }

    return spdk_dma_malloc(size, alignment, NULL);
}

void xpdk_free_buffer(void *buffer)
{
    if (buffer != NULL) {
        spdk_dma_free(buffer);
    }
}
