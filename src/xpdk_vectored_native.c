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
        msg->io.bytes_transferred = spdk_bdev_io_get_num_blocks(bdev_io) * 
                                   spdk_bdev_get_block_size(spdk_bdev_io_get_bdev(bdev_io));
        
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
        msg->io.bytes_transferred = spdk_bdev_io_get_num_blocks(bdev_io) * 
                                   spdk_bdev_get_block_size(spdk_bdev_io_get_bdev(bdev_io));
        
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
