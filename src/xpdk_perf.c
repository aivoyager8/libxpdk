#include "xpdk_internal.h"
#include <string.h>
#include <sys/time.h>

/* Performance statistics and monitoring */

/* Get current time in microseconds */
static uint64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/* Initialize performance statistics for a device */
int xpdk_perf_stats_init(struct xpdk_device *dev)
{
    memset(&dev->perf_stats, 0, sizeof(dev->perf_stats));
    dev->stats_start_time = get_time_us();
    return XPDK_SUCCESS;
}

/* Update performance statistics after I/O operation */
void xpdk_perf_stats_update(struct xpdk_device *dev, xpdk_io_type_t io_type, 
                           size_t bytes, uint64_t latency_us, bool success)
{
    if (!dev) return;

    struct xpdk_perf_stats *stats = &dev->perf_stats;
    
    if (success) {
        if (io_type == XPDK_IO_READ) {
            stats->total_read_ops++;
            stats->total_bytes_read += bytes;
            
            /* Update average read latency (exponential moving average) */
            if (stats->total_read_ops == 1) {
                stats->avg_read_latency_us = latency_us;
            } else {
                stats->avg_read_latency_us = (stats->avg_read_latency_us * 7 + latency_us) / 8;
            }
        } else if (io_type == XPDK_IO_WRITE) {
            stats->total_write_ops++;
            stats->total_bytes_written += bytes;
            
            /* Update average write latency (exponential moving average) */
            if (stats->total_write_ops == 1) {
                stats->avg_write_latency_us = latency_us;
            } else {
                stats->avg_write_latency_us = (stats->avg_write_latency_us * 7 + latency_us) / 8;
            }
        }
    } else {
        stats->errors++;
    }

    /* Update current IOPS and bandwidth (calculated over last second) */
    uint64_t current_time = get_time_us();
    uint64_t time_window = current_time - dev->stats_window_start;
    
    if (time_window >= 1000000) { /* 1 second window */
        uint64_t total_ops = stats->total_read_ops + stats->total_write_ops;
        uint64_t total_bytes = stats->total_bytes_read + stats->total_bytes_written;
        
        stats->current_iops = (total_ops - dev->last_total_ops) * 1000000 / time_window;
        stats->current_bandwidth = (total_bytes - dev->last_total_bytes) * 1000000 / time_window;
        
        dev->last_total_ops = total_ops;
        dev->last_total_bytes = total_bytes;
        dev->stats_window_start = current_time;
    }
}

/* Get performance statistics for a device */
int xpdk_get_perf_stats(xpdk_fd_t fd, struct xpdk_perf_stats *stats)
{
    struct xpdk_msg *msg;
    
    if (!stats) {
        return XPDK_ERROR_INVALID;
    }

    msg = xpdk_msg_alloc(XPDK_MSG_GET_PERF_STATS);
    if (!msg) {
        return XPDK_ERROR_NOMEM;
    }

    msg->perf_stats.fd = fd;
    msg->perf_stats.stats = stats;

    int rc = xpdk_msg_send_sync(msg);
    xpdk_msg_free(msg);
    return rc;
}

/* Reset performance statistics for a device */
int xpdk_reset_perf_stats(xpdk_fd_t fd)
{
    struct xpdk_msg *msg;

    msg = xpdk_msg_alloc(XPDK_MSG_RESET_PERF_STATS);
    if (!msg) {
        return XPDK_ERROR_NOMEM;
    }

    msg->perf_stats.fd = fd;
    msg->perf_stats.stats = NULL;

    int rc = xpdk_msg_send_sync(msg);
    xpdk_msg_free(msg);
    return rc;
}

/* SPDK thread handler for get performance stats */
void xpdk_spdk_handle_get_perf_stats(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->perf_stats.fd);
    
    if (!dev) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }

    /* Update current statistics before returning */
    uint64_t current_time = get_time_us();
    uint64_t uptime = current_time - dev->stats_start_time;
    
    struct xpdk_perf_stats *stats = &dev->perf_stats;
    if (uptime > 0) {
        /* Calculate overall averages */
        uint64_t total_ops = stats->total_read_ops + stats->total_write_ops;
        if (total_ops > 0) {
            stats->current_iops = total_ops * 1000000 / uptime;
        }
        
        uint64_t total_bytes = stats->total_bytes_read + stats->total_bytes_written;
        if (total_bytes > 0) {
            stats->current_bandwidth = total_bytes * 1000000 / uptime;
        }
    }

    /* Copy statistics to user buffer */
    *msg->perf_stats.stats = dev->perf_stats;
    
    msg->status = XPDK_SUCCESS;
    msg->completed = true;
}

/* SPDK thread handler for reset performance stats */
void xpdk_spdk_handle_reset_perf_stats(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->perf_stats.fd);
    
    if (!dev) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }

    /* Reset all statistics */
    memset(&dev->perf_stats, 0, sizeof(dev->perf_stats));
    dev->stats_start_time = get_time_us();
    dev->stats_window_start = dev->stats_start_time;
    dev->last_total_ops = 0;
    dev->last_total_bytes = 0;
    
    msg->status = XPDK_SUCCESS;
    msg->completed = true;
}

/* Storage space management functions */

/* Trim/unmap storage space */
int xpdk_trim(xpdk_fd_t fd, uint64_t offset, uint64_t length)
{
    struct xpdk_msg *msg;

    msg = xpdk_msg_alloc(XPDK_MSG_TRIM);
    if (!msg) {
        return XPDK_ERROR_NOMEM;
    }

    msg->trim.fd = fd;
    msg->trim.offset = offset;
    msg->trim.length = length;

    int rc = xpdk_msg_send_sync(msg);
    xpdk_msg_free(msg);
    return rc;
}

/* Write zeros to storage space */
int xpdk_write_zeros(xpdk_fd_t fd, uint64_t offset, uint64_t length)
{
    struct xpdk_msg *msg;

    msg = xpdk_msg_alloc(XPDK_MSG_WRITE_ZEROS);
    if (!msg) {
        return XPDK_ERROR_NOMEM;
    }

    msg->trim.fd = fd;
    msg->trim.offset = offset;
    msg->trim.length = length;

    int rc = xpdk_msg_send_sync(msg);
    xpdk_msg_free(msg);
    return rc;
}

/* SPDK thread handler for trim operation */
void xpdk_spdk_handle_trim(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->trim.fd);
    
    if (!dev) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }

    uint32_t block_size = spdk_bdev_get_block_size(dev->bdev);
    uint64_t offset_blocks = msg->trim.offset / block_size;
    uint32_t num_blocks = (msg->trim.length + block_size - 1) / block_size;

    /* Use SPDK's unmap function for trim operation */
    int rc = spdk_bdev_unmap_blocks(dev->desc, dev->channel, offset_blocks, num_blocks,
                                   NULL, NULL);
    
    msg->status = (rc == 0) ? XPDK_SUCCESS : XPDK_ERROR_IO;
    msg->completed = true;
}

/* SPDK thread handler for write zeros operation */
void xpdk_spdk_handle_write_zeros(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->trim.fd);
    
    if (!dev) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }

    uint32_t block_size = spdk_bdev_get_block_size(dev->bdev);
    uint64_t offset_blocks = msg->trim.offset / block_size;
    uint32_t num_blocks = (msg->trim.length + block_size - 1) / block_size;

    /* Use SPDK's write zeros function */
    int rc = spdk_bdev_write_zeroes_blocks(dev->desc, dev->channel, offset_blocks, num_blocks,
                                          NULL, NULL);
    
    msg->status = (rc == 0) ? XPDK_SUCCESS : XPDK_ERROR_IO;
    msg->completed = true;
}
