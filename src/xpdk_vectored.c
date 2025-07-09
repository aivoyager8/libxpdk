#include "xpdk_internal.h"
#include <string.h>
#include <stdlib.h>

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

/* Vectored read operation (synchronous) */
ssize_t xpdk_readv(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (iov == NULL || iovcnt <= 0) {
        return XPDK_ERROR_INVALID;
    }

    /* Calculate total size */
    size_t total_size = calculate_iovec_size(iov, iovcnt);
    if (total_size == 0) {
        return XPDK_ERROR_INVALID;
    }

    /* Allocate temporary linear buffer */
    void *temp_buffer = xpdk_alloc_buffer(total_size, 0);
    if (temp_buffer == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    /* Perform regular read into temporary buffer */
    ssize_t bytes_read = xpdk_read(fd, temp_buffer, total_size, offset);
    
    if (bytes_read > 0) {
        /* Copy data from linear buffer to iovec */
        buffer_to_iovec(temp_buffer, iov, iovcnt);
    }

    xpdk_free_buffer(temp_buffer);
    return bytes_read;
}

/* Vectored write operation (synchronous) */
ssize_t xpdk_writev(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (iov == NULL || iovcnt <= 0) {
        return XPDK_ERROR_INVALID;
    }

    /* Calculate total size */
    size_t total_size = calculate_iovec_size(iov, iovcnt);
    if (total_size == 0) {
        return XPDK_ERROR_INVALID;
    }

    /* Allocate temporary linear buffer */
    void *temp_buffer = xpdk_alloc_buffer(total_size, 0);
    if (temp_buffer == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    /* Copy data from iovec to linear buffer */
    iovec_to_buffer(iov, iovcnt, temp_buffer);

    /* Perform regular write from temporary buffer */
    ssize_t bytes_written = xpdk_write(fd, temp_buffer, total_size, offset);

    xpdk_free_buffer(temp_buffer);
    return bytes_written;
}

/* Asynchronous vectored read operation */
int xpdk_readv_async(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset,
                     xpdk_io_callback_t callback, void *ctx)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (iov == NULL || iovcnt <= 0 || callback == NULL) {
        return XPDK_ERROR_INVALID;
    }

    /* Calculate total size */
    size_t total_size = calculate_iovec_size(iov, iovcnt);
    if (total_size == 0) {
        return XPDK_ERROR_INVALID;
    }

    /* Allocate temporary linear buffer */
    void *temp_buffer = xpdk_alloc_buffer(total_size, 0);
    if (temp_buffer == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    /* Create wrapper context for async operation */
    struct xpdk_vectored_ctx *v_ctx = malloc(sizeof(struct xpdk_vectored_ctx));
    if (v_ctx == NULL) {
        xpdk_free_buffer(temp_buffer);
        return XPDK_ERROR_NOMEM;
    }

    v_ctx->original_callback = callback;
    v_ctx->original_ctx = ctx;
    v_ctx->iov = iov;
    v_ctx->iovcnt = iovcnt;
    v_ctx->temp_buffer = temp_buffer;
    v_ctx->is_read = true;

    /* Perform async read with wrapper callback */
    return xpdk_read_async(fd, temp_buffer, total_size, offset, 
                          xpdk_vectored_completion_callback, v_ctx);
}

/* Asynchronous vectored write operation */
int xpdk_writev_async(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset,
                      xpdk_io_callback_t callback, void *ctx)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (iov == NULL || iovcnt <= 0 || callback == NULL) {
        return XPDK_ERROR_INVALID;
    }

    /* Calculate total size */
    size_t total_size = calculate_iovec_size(iov, iovcnt);
    if (total_size == 0) {
        return XPDK_ERROR_INVALID;
    }

    /* Allocate temporary linear buffer */
    void *temp_buffer = xpdk_alloc_buffer(total_size, 0);
    if (temp_buffer == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    /* Copy data from iovec to linear buffer */
    iovec_to_buffer(iov, iovcnt, temp_buffer);

    /* Create wrapper context for async operation */
    struct xpdk_vectored_ctx *v_ctx = malloc(sizeof(struct xpdk_vectored_ctx));
    if (v_ctx == NULL) {
        xpdk_free_buffer(temp_buffer);
        return XPDK_ERROR_NOMEM;
    }

    v_ctx->original_callback = callback;
    v_ctx->original_ctx = ctx;
    v_ctx->iov = iov;
    v_ctx->iovcnt = iovcnt;
    v_ctx->temp_buffer = temp_buffer;
    v_ctx->is_read = false;

    /* Perform async write with wrapper callback */
    return xpdk_write_async(fd, temp_buffer, total_size, offset, 
                           xpdk_vectored_completion_callback, v_ctx);
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
