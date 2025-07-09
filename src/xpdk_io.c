#include "xpdk_internal.h"
#include <string.h>
#include <fcntl.h>
#include <spdk/bdev.h>

/* I/O completion context for synchronous operations */
struct sync_io_ctx {
    struct xpdk_msg *msg;
    volatile bool completed;
    int status;
    size_t bytes_transferred;
};

/* I/O completion callback for synchronous operations */
static void
sync_io_completion_cb(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
    struct sync_io_ctx *ctx = (struct sync_io_ctx *)cb_arg;
    
    ctx->completed = true;
    ctx->status = success ? XPDK_SUCCESS : XPDK_ERROR_IO;
    
    /* Update message status */
    ctx->msg->status = ctx->status;
    ctx->msg->io.bytes_transferred = success ? ctx->bytes_transferred : 0;
    ctx->msg->completed = true;
    
    spdk_bdev_free_io(bdev_io);
}

/* I/O completion callback for asynchronous operations */
static void
async_io_completion_cb(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
    struct xpdk_msg *msg = (struct xpdk_msg *)cb_arg;
    
    int status = success ? XPDK_SUCCESS : XPDK_ERROR_IO;
    
    /* Call user callback if this is an async operation */
    if (msg->io.callback != NULL) {
        msg->io.callback(msg->io.user_ctx, status);
    }
    
    /* Free aligned buffer if allocated */
    if (msg->ctx != NULL) {
        spdk_dma_free(msg->ctx);
    }
    
    /* Free message */
    xpdk_msg_free(msg);
    
    spdk_bdev_free_io(bdev_io);
}

/* Handle read request in SPDK thread */
void
xpdk_spdk_handle_read(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->io.fd);
    if (dev == NULL || dev->bdev == NULL || dev->desc == NULL || dev->channel == NULL) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }

    /* Check read permission */
    if ((dev->flags & O_WRONLY)) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }

    uint32_t block_size = spdk_bdev_get_block_size(dev->bdev);
    uint64_t offset_blocks = msg->io.offset / block_size;
    uint32_t num_blocks = (msg->io.count + block_size - 1) / block_size;

    void *buffer = msg->io.buffer;
    bool need_aligned_buffer = false;
    
    /* Check if we need aligned buffer */
    if ((uintptr_t)buffer % block_size != 0 || msg->io.count % block_size != 0) {
        buffer = spdk_dma_malloc(num_blocks * block_size, block_size, NULL);
        if (buffer == NULL) {
            msg->status = XPDK_ERROR_NOMEM;
            msg->completed = true;
            return;
        }
        need_aligned_buffer = true;
        msg->ctx = buffer; /* Store for later cleanup */
    }

    /* Prepare completion context */
    struct sync_io_ctx ctx = {
        .msg = msg,
        .completed = false,
        .status = XPDK_SUCCESS,
        .bytes_transferred = msg->io.count
    };

    /* Submit read I/O */
    int rc = spdk_bdev_read_blocks(dev->desc, dev->channel, buffer,
                                   offset_blocks, num_blocks,
                                   msg->io.callback ? async_io_completion_cb : sync_io_completion_cb,
                                   msg->io.callback ? (void *)msg : (void *)&ctx);
    if (rc != 0) {
        if (need_aligned_buffer) {
            spdk_dma_free(buffer);
        }
        msg->status = XPDK_ERROR_IO;
        msg->completed = true;
        return;
    }

    /* For synchronous operations, wait for completion */
    if (!msg->io.callback) {
        while (!ctx.completed) {
            spdk_thread_poll(g_xpdk_ctx.spdk_thread, 0, 0);
        }
        
        /* Copy data if we used aligned buffer */
        if (need_aligned_buffer && ctx.status == XPDK_SUCCESS) {
            memcpy(msg->io.buffer, buffer, msg->io.count);
            spdk_dma_free(buffer);
        }
    }
    /* For async operations, completion callback will handle cleanup */
}

/* Handle write request in SPDK thread */
void
xpdk_spdk_handle_write(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->io.fd);
    if (dev == NULL || dev->bdev == NULL || dev->desc == NULL || dev->channel == NULL) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }

    /* Check write permission */
    if ((dev->flags & O_RDONLY)) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }

    uint32_t block_size = spdk_bdev_get_block_size(dev->bdev);
    uint64_t offset_blocks = msg->io.offset / block_size;
    uint32_t num_blocks = (msg->io.count + block_size - 1) / block_size;

    /* Always use aligned buffer for write */
    void *buffer = spdk_dma_malloc(num_blocks * block_size, block_size, NULL);
    if (buffer == NULL) {
        msg->status = XPDK_ERROR_NOMEM;
        msg->completed = true;
        return;
    }

    memset(buffer, 0, num_blocks * block_size);
    memcpy(buffer, msg->io.buffer, msg->io.count);
    msg->ctx = buffer; /* Store for cleanup */

    /* Prepare completion context */
    struct sync_io_ctx ctx = {
        .msg = msg,
        .completed = false,
        .status = XPDK_SUCCESS,
        .bytes_transferred = msg->io.count
    };

    /* Submit write I/O */
    int rc = spdk_bdev_write_blocks(dev->desc, dev->channel, buffer,
                                    offset_blocks, num_blocks,
                                    msg->io.callback ? async_io_completion_cb : sync_io_completion_cb,
                                    msg->io.callback ? (void *)msg : (void *)&ctx);
    if (rc != 0) {
        spdk_dma_free(buffer);
        msg->status = XPDK_ERROR_IO;
        msg->completed = true;
        return;
    }

    /* For synchronous operations, wait for completion */
    if (!msg->io.callback) {
        while (!ctx.completed) {
            spdk_thread_poll(g_xpdk_ctx.spdk_thread, 0, 0);
        }
        spdk_dma_free(buffer);
    }
    /* For async operations, completion callback will handle cleanup */
}

/* Handle flush request in SPDK thread */
void
xpdk_spdk_handle_flush(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->io.fd);
    if (dev == NULL || dev->bdev == NULL || dev->desc == NULL || dev->channel == NULL) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }

    /* Prepare completion context */
    struct sync_io_ctx ctx = {
        .msg = msg,
        .completed = false,
        .status = XPDK_SUCCESS,
        .bytes_transferred = 0
    };

    /* Submit flush I/O */
    int rc = spdk_bdev_flush_blocks(dev->desc, dev->channel, 0,
                                    spdk_bdev_get_num_blocks(dev->bdev),
                                    sync_io_completion_cb, &ctx);
    if (rc != 0) {
        msg->status = XPDK_ERROR_IO;
        msg->completed = true;
        return;
    }

    /* Wait for completion */
    while (!ctx.completed) {
        spdk_thread_poll(g_xpdk_ctx.spdk_thread, 0, 0);
    }
}

/* Public API functions */

ssize_t
xpdk_read(xpdk_fd_t fd, void *buffer, size_t count, uint64_t offset)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (buffer == NULL || count == 0) {
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
    msg->io.callback = NULL; /* Synchronous */

    int rc = xpdk_msg_send_sync(msg);
    ssize_t result = (rc == XPDK_SUCCESS) ? (ssize_t)msg->io.bytes_transferred : rc;

    xpdk_msg_free(msg);
    return result;
}

ssize_t
xpdk_write(xpdk_fd_t fd, const void *buffer, size_t count, uint64_t offset)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (buffer == NULL || count == 0) {
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
    msg->io.callback = NULL; /* Synchronous */

    int rc = xpdk_msg_send_sync(msg);
    ssize_t result = (rc == XPDK_SUCCESS) ? (ssize_t)msg->io.bytes_transferred : rc;

    xpdk_msg_free(msg);
    return result;
}

int
xpdk_flush(xpdk_fd_t fd)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_FLUSH);
    if (msg == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    msg->io.fd = fd;

    int rc = xpdk_msg_send_sync(msg);

    xpdk_msg_free(msg);
    return rc;
}
