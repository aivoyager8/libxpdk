/* High-performance vectored I/O operations using native SPDK APIs */
#include "xpdk_internal.h"
#include <spdk/bdev.h>
#include <stdlib.h>

/* Pre-allocated iovec pool for high performance */
static struct spdk_mempool *g_iovec_pool = NULL;

/* Initialize iovec memory pool */
int xpdk_iovec_pool_init(void)
{
    g_iovec_pool = spdk_mempool_create("xpdk_iovec_pool",
                                       1024,  /* pool size */
                                       sizeof(struct iovec) * 32,  /* max 32 iovecs */
                                       SPDK_MEMPOOL_DEFAULT_CACHE_SIZE,
                                       SPDK_ENV_SOCKET_ID_ANY);
    return g_iovec_pool ? 0 : -1;
}

/* Cleanup iovec memory pool */
void xpdk_iovec_pool_cleanup(void)
{
    if (g_iovec_pool) {
        spdk_mempool_free(g_iovec_pool);
        g_iovec_pool = NULL;
    }
}

/* Allocate iovec from pool - high performance */
struct iovec *xpdk_alloc_iovec(int count)
{
    if (count <= 0 || count > 32) {
        return NULL;
    }
    
    if (g_iovec_pool) {
        return spdk_mempool_get(g_iovec_pool);
    }
    
    /* Fallback to malloc if pool not available */
    return malloc(sizeof(struct iovec) * count);
}

/* Free iovec to pool - high performance */
void xpdk_free_iovec(struct iovec *iov)
{
    if (!iov) return;
    
    if (g_iovec_pool) {
        spdk_mempool_put(g_iovec_pool, iov);
    } else {
        free(iov);
    }
}

/* Convert xpdk_iovec to spdk_iovec - optimized */
static inline void convert_iovec_fast(const struct xpdk_iovec *xiov, int iovcnt, struct iovec *siov)
{
    for (int i = 0; i < iovcnt; i++) {
        siov[i].iov_base = xiov[i].iov_base;
        siov[i].iov_len = xiov[i].iov_len;
    }
}

/* High-performance vectored read completion callback */
static void vectored_read_completion_cb(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
    struct xpdk_vectored_async_ctx *ctx = (struct xpdk_vectored_async_ctx *)cb_arg;
    struct xpdk_msg *msg = ctx->msg;
    
    /* Calculate performance stats */
    uint64_t end_time = xpdk_get_time_us();
    uint64_t latency = end_time - ctx->start_time;
    
    if (success) {
        msg->status = XPDK_SUCCESS;
        // Note: Use the original bytes_transferred from the request
        // as SPDK API for getting transferred bytes from bdev_io is not available
        msg->io.bytes_transferred = msg->io.count;  // count stores total bytes for vectored IO
        
        /* Update performance statistics */
        struct xpdk_device *dev = xpdk_get_device(msg->io.fd);
        if (dev) {
            xpdk_perf_stats_update(dev, XPDK_IO_READ, msg->io.bytes_transferred, latency, true);
        }
    } else {
        msg->status = XPDK_ERROR_IO;
        msg->io.bytes_transferred = 0;
        
        struct xpdk_device *dev = xpdk_get_device(msg->io.fd);
        if (dev) {
            xpdk_perf_stats_update(dev, XPDK_IO_READ, 0, latency, false);
        }
    }
    
    spdk_bdev_free_io(bdev_io);
    
    /* Free SPDK resources */
    xpdk_free_iovec(ctx->spdk_iov);
    
    /* Handle async callback if present */
    if (msg->io.callback) {
        msg->io.callback(msg->io.user_ctx, msg->status);
        xpdk_msg_free(msg);
    } else {
        msg->completed = true;  /* For sync operations */
    }
    
    free(ctx);
}

/* High-performance vectored write completion callback */
static void vectored_write_completion_cb(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
    struct xpdk_vectored_async_ctx *ctx = (struct xpdk_vectored_async_ctx *)cb_arg;
    struct xpdk_msg *msg = ctx->msg;
    
    /* Calculate performance stats */
    uint64_t end_time = xpdk_get_time_us();
    uint64_t latency = end_time - ctx->start_time;
    
    if (success) {
        msg->status = XPDK_SUCCESS;
        // Note: Use the original bytes_transferred from the request
        // as SPDK API for getting transferred bytes from bdev_io is not available
        msg->io.bytes_transferred = msg->io.count;  // count stores total bytes for vectored IO
        
        /* Update performance statistics */
        struct xpdk_device *dev = xpdk_get_device(msg->io.fd);
        if (dev) {
            xpdk_perf_stats_update(dev, XPDK_IO_WRITE, msg->io.bytes_transferred, latency, true);
        }
    } else {
        msg->status = XPDK_ERROR_IO;
        msg->io.bytes_transferred = 0;
        
        struct xpdk_device *dev = xpdk_get_device(msg->io.fd);
        if (dev) {
            xpdk_perf_stats_update(dev, XPDK_IO_WRITE, 0, latency, false);
        }
    }
    
    spdk_bdev_free_io(bdev_io);
    
    /* Free SPDK resources */
    xpdk_free_iovec(ctx->spdk_iov);
    
    /* Handle async callback if present */
    if (msg->io.callback) {
        msg->io.callback(msg->io.user_ctx, msg->status);
        xpdk_msg_free(msg);
    } else {
        msg->completed = true;  /* For sync operations */
    }
    
    free(ctx);
}

/* SPDK thread handler for high-performance native vectored read */
void xpdk_spdk_handle_readv_native(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->io.fd);
    
    if (!dev || !dev->bdev || !dev->desc || !dev->channel) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }
    
    const struct xpdk_iovec *xiov = (const struct xpdk_iovec *)msg->io.buffer;
    int iovcnt = (int)msg->io.count;
    uint64_t offset = msg->io.offset;
    
    /* Fast validation */
    if (!xiov || iovcnt <= 0 || iovcnt > 32) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }
    
    /* Allocate SPDK iovec from high-performance pool */
    struct iovec *siov = xpdk_alloc_iovec(iovcnt);
    if (!siov) {
        msg->status = XPDK_ERROR_NOMEM;
        msg->completed = true;
        return;
    }
    
    /* Fast iovec conversion */
    convert_iovec_fast(xiov, iovcnt, siov);
    
    /* Calculate total size and convert to blocks */
    size_t total_size = 0;
    for (int i = 0; i < iovcnt; i++) {
        total_size += xiov[i].iov_len;
    }
    
    uint32_t block_size = spdk_bdev_get_block_size(dev->bdev);
    uint64_t offset_blocks = offset / block_size;
    uint64_t num_blocks = (total_size + block_size - 1) / block_size;
    
    /* Fast alignment validation */
    if (offset % block_size != 0) {
        xpdk_free_iovec(siov);
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }
    
    /* Create high-performance async context */
    struct xpdk_vectored_async_ctx *ctx = malloc(sizeof(struct xpdk_vectored_async_ctx));
    if (!ctx) {
        xpdk_free_iovec(siov);
        msg->status = XPDK_ERROR_NOMEM;
        msg->completed = true;
        return;
    }
    
    ctx->msg = msg;
    ctx->spdk_iov = siov;
    ctx->iovcnt = iovcnt;
    ctx->start_time = xpdk_get_time_us();
    
    /* Submit high-performance vectored read using SPDK native API */
    int rc = spdk_bdev_readv_blocks(dev->desc, dev->channel, siov, iovcnt,
                                    offset_blocks, num_blocks,
                                    vectored_read_completion_cb, ctx);
    
    if (rc != 0) {
        xpdk_free_iovec(siov);
        free(ctx);
        msg->status = XPDK_ERROR_IO;
        msg->completed = true;
        return;
    }
    
    /* For sync operations, completion will be handled by callback */
    /* For async operations, callback will be called by completion handler */
}

/* SPDK thread handler for high-performance native vectored write */
void xpdk_spdk_handle_writev_native(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->io.fd);
    
    if (!dev || !dev->bdev || !dev->desc || !dev->channel) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }
    
    const struct xpdk_iovec *xiov = (const struct xpdk_iovec *)msg->io.buffer;
    int iovcnt = (int)msg->io.count;
    uint64_t offset = msg->io.offset;
    
    /* Fast validation */
    if (!xiov || iovcnt <= 0 || iovcnt > 32) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }
    
    /* Allocate SPDK iovec from high-performance pool */
    struct iovec *siov = xpdk_alloc_iovec(iovcnt);
    if (!siov) {
        msg->status = XPDK_ERROR_NOMEM;
        msg->completed = true;
        return;
    }
    
    /* Fast iovec conversion */
    convert_iovec_fast(xiov, iovcnt, siov);
    
    /* Calculate total size and convert to blocks */
    size_t total_size = 0;
    for (int i = 0; i < iovcnt; i++) {
        total_size += xiov[i].iov_len;
    }
    
    uint32_t block_size = spdk_bdev_get_block_size(dev->bdev);
    uint64_t offset_blocks = offset / block_size;
    uint64_t num_blocks = (total_size + block_size - 1) / block_size;
    
    /* Fast alignment validation */
    if (offset % block_size != 0) {
        xpdk_free_iovec(siov);
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }
    
    /* Create high-performance async context */
    struct xpdk_vectored_async_ctx *ctx = malloc(sizeof(struct xpdk_vectored_async_ctx));
    if (!ctx) {
        xpdk_free_iovec(siov);
        msg->status = XPDK_ERROR_NOMEM;
        msg->completed = true;
        return;
    }
    
    ctx->msg = msg;
    ctx->spdk_iov = siov;
    ctx->iovcnt = iovcnt;
    ctx->start_time = xpdk_get_time_us();
    
    /* Submit high-performance vectored write using SPDK native API */
    int rc = spdk_bdev_writev_blocks(dev->desc, dev->channel, siov, iovcnt,
                                     offset_blocks, num_blocks,
                                     vectored_write_completion_cb, ctx);
    
    if (rc != 0) {
        xpdk_free_iovec(siov);
        free(ctx);
        msg->status = XPDK_ERROR_IO;
        msg->completed = true;
        return;
    }
    
    /* For sync operations, completion will be handled by callback */
    /* For async operations, callback will be called by completion handler */
}

/* Public API functions for vectored I/O */

/* High-performance vectored read operation (synchronous) */
ssize_t xpdk_readv(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (iov == NULL || iovcnt <= 0 || iovcnt > 32) {
        return XPDK_ERROR_INVALID;
    }

    struct xpdk_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = XPDK_MSG_READV_NATIVE;
    msg.io.fd = fd;
    msg.io.offset = offset;
    msg.io.buffer = (void *)iov;  // Store iovec array in buffer field
    msg.io.count = iovcnt;        // Store iovec count in count field
    
    /* Calculate total size and store in spdk_ctx (reuse as temp storage) */
    size_t total_size = 0;
    for (int i = 0; i < iovcnt; i++) {
        total_size += iov[i].iov_len;
    }
    msg.io.spdk_ctx = (void *)total_size;  // Store total size temporarily
    
    /* Send to SPDK thread */
    xpdk_msg_send(&msg);
    
    /* Wait for completion */
    xpdk_msg_wait(&msg);
    
    if (msg.status == XPDK_SUCCESS) {
        return msg.io.bytes_transferred;
    } else {
        return msg.status;
    }
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

    struct xpdk_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = XPDK_MSG_WRITEV_NATIVE;
    msg.io.fd = fd;
    msg.io.offset = offset;
    msg.io.buffer = (void *)iov;  // Store iovec array in buffer field
    msg.io.count = iovcnt;        // Store iovec count in count field
    
    /* Calculate total size and store in spdk_ctx (reuse as temp storage) */
    size_t total_size = 0;
    for (int i = 0; i < iovcnt; i++) {
        total_size += iov[i].iov_len;
    }
    msg.io.spdk_ctx = (void *)total_size;  // Store total size temporarily
    
    /* Send to SPDK thread */
    xpdk_msg_send(&msg);
    
    /* Wait for completion */
    xpdk_msg_wait(&msg);
    
    if (msg.status == XPDK_SUCCESS) {
        return msg.io.bytes_transferred;
    } else {
        return msg.status;
    }
}

/* Asynchronous vectored read operation */
int xpdk_readv_async(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset,
                     xpdk_io_callback_t callback, void *ctx)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (iov == NULL || iovcnt <= 0 || iovcnt > 32 || callback == NULL) {
        return XPDK_ERROR_INVALID;
    }

    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_READV_NATIVE);
    if (!msg) {
        return XPDK_ERROR_NOMEM;
    }

    msg->io.fd = fd;
    msg->io.offset = offset;
    msg->io.buffer = (void *)iov;  // Store iovec array in buffer field
    msg->io.count = iovcnt;        // Store iovec count in count field
    msg->io.callback = callback;
    msg->io.user_ctx = ctx;
    
    /* Calculate total size and store in spdk_ctx (reuse as temp storage) */
    size_t total_size = 0;
    for (int i = 0; i < iovcnt; i++) {
        total_size += iov[i].iov_len;
    }
    msg->io.spdk_ctx = (void *)total_size;  // Store total size temporarily
    
    /* Send to SPDK thread */
    xpdk_msg_send(msg);
    
    return XPDK_SUCCESS;
}

/* Asynchronous vectored write operation */
int xpdk_writev_async(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset,
                      xpdk_io_callback_t callback, void *ctx)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (iov == NULL || iovcnt <= 0 || iovcnt > 32 || callback == NULL) {
        return XPDK_ERROR_INVALID;
    }

    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_WRITEV_NATIVE);
    if (!msg) {
        return XPDK_ERROR_NOMEM;
    }

    msg->io.fd = fd;
    msg->io.offset = offset;
    msg->io.buffer = (void *)iov;  // Store iovec array in buffer field
    msg->io.count = iovcnt;        // Store iovec count in count field
    msg->io.callback = callback;
    msg->io.user_ctx = ctx;
    
    /* Calculate total size and store in spdk_ctx (reuse as temp storage) */
    size_t total_size = 0;
    for (int i = 0; i < iovcnt; i++) {
        total_size += iov[i].iov_len;
    }
    msg->io.spdk_ctx = (void *)total_size;  // Store total size temporarily
    
    /* Send to SPDK thread */
    xpdk_msg_send(msg);
    
    return XPDK_SUCCESS;
}
