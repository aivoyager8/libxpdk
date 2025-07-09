#include "xpdk_internal.h"
#include <string.h>
#include <fcntl.h>
#include <spdk/bdev.h>

/* SPDK thread operation handlers */

struct bdev_list_ctx {
    struct xpdk_bdev_info *devices;
    int max_devices;
    int count;
};

static int
bdev_list_iter(void *ctx, struct spdk_bdev *bdev)
{
    struct bdev_list_ctx *list_ctx = (struct bdev_list_ctx *)ctx;
    
    if (list_ctx->count >= list_ctx->max_devices) {
        return 0;  // Continue iteration
    }

    struct xpdk_bdev_info *info = &list_ctx->devices[list_ctx->count];
    
    /* Copy device name */
    strncpy(info->name, spdk_bdev_get_name(bdev), sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    
    /* Get device properties */
    info->block_size = spdk_bdev_get_block_size(bdev);
    info->num_blocks = spdk_bdev_get_num_blocks(bdev);
    info->capacity = info->block_size * info->num_blocks;
    
    list_ctx->count++;
    return 0;  // Continue iteration
}

/* Handle list bdevs request in SPDK thread */
void
xpdk_spdk_handle_list_bdevs(struct xpdk_msg *msg)
{
    struct bdev_list_ctx ctx = {
        .devices = msg->list.devices,
        .max_devices = msg->list.max_devices,
        .count = 0
    };

    /* Iterate through all block devices */
    spdk_for_each_bdev(&ctx, bdev_list_iter);
    
    msg->list.count = ctx.count;
    msg->status = XPDK_SUCCESS;
    msg->completed = true;
}

static void
bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev, void *event_ctx)
{
    /* Handle bdev events like removal */
    // For now, we'll just log and continue
}

static void
bdev_open_complete(struct spdk_bdev_desc *desc, int status, void *cb_arg)
{
    struct xpdk_msg *msg = (struct xpdk_msg *)cb_arg;
    
    if (status != 0) {
        msg->status = XPDK_ERROR_IO;
        msg->completed = true;
        return;
    }
    
    /* Store descriptor and mark complete */
    struct xpdk_device *dev = &g_xpdk_ctx.devices[msg->open.result_fd];
    dev->desc = desc;
    
    /* Get I/O channel */
    dev->channel = spdk_bdev_get_io_channel(desc);
    if (dev->channel == NULL) {
        spdk_bdev_close(desc);
        dev->desc = NULL;
        msg->status = XPDK_ERROR_IO;
        msg->completed = true;
        return;
    }
    
    /* Initialize performance statistics */
    xpdk_perf_stats_init(dev);

    msg->status = XPDK_SUCCESS;
    msg->completed = true;
}

/* Handle open request in SPDK thread */
void
xpdk_spdk_handle_open(struct xpdk_msg *msg)
{
    /* Find the block device */
    struct spdk_bdev *bdev = spdk_bdev_get_by_name(msg->open.bdev_name);
    if (bdev == NULL) {
        msg->status = XPDK_ERROR_NODEV;
        msg->completed = true;
        return;
    }

    /* Find free file descriptor */
    int fd = xpdk_find_free_fd();
    if (fd < 0) {
        msg->status = fd;
        msg->completed = true;
        return;
    }

    struct xpdk_device *dev = &g_xpdk_ctx.devices[fd];
    dev->bdev = bdev;
    dev->flags = msg->open.flags;
    msg->open.result_fd = fd;

    /* Open the device asynchronously */
    bool write_access = (msg->open.flags & O_RDWR) || (msg->open.flags & O_WRONLY);
    
    // Use spdk_bdev_open which is simpler API
    int rc = spdk_bdev_open_ext(msg->open.bdev_name, write_access, 
                               bdev_event_cb, msg, &dev->desc);
    if (rc != 0) {
        xpdk_put_device(dev);
        msg->status = XPDK_ERROR_IO;
        msg->completed = true;
        return;
    }
    
    // Since we got the desc immediately, complete the operation
    msg->status = XPDK_SUCCESS;
    msg->completed = true;
    return;
}

/* Handle close request in SPDK thread */
void
xpdk_spdk_handle_close(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->close.fd);
    if (dev == NULL) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }

    /* Close I/O channel */
    if (dev->channel != NULL) {
        spdk_put_io_channel(dev->channel);
        dev->channel = NULL;
    }

    /* Close device descriptor */
    if (dev->desc != NULL) {
        spdk_bdev_close(dev->desc);
        dev->desc = NULL;
    }

    dev->bdev = NULL;
    dev->flags = 0;
    xpdk_put_device(dev);

    msg->status = XPDK_SUCCESS;
    msg->completed = true;
}

/* Handle get info request in SPDK thread */
void
xpdk_spdk_handle_get_info(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->get_info.fd);
    if (dev == NULL || dev->bdev == NULL) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }

    struct xpdk_bdev_info *info = msg->get_info.info;
    
    /* Fill device information */
    strncpy(info->name, spdk_bdev_get_name(dev->bdev), sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    
    info->block_size = spdk_bdev_get_block_size(dev->bdev);
    info->num_blocks = spdk_bdev_get_num_blocks(dev->bdev);
    info->capacity = info->block_size * info->num_blocks;

    msg->status = XPDK_SUCCESS;
    msg->completed = true;
}

/* Public API functions that send messages to SPDK thread */

int
xpdk_list_bdevs(struct xpdk_bdev_info *devices, int max_devices)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (devices == NULL || max_devices <= 0) {
        return XPDK_ERROR_INVALID;
    }

    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_LIST_BDEVS);
    if (msg == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    msg->list.devices = devices;
    msg->list.max_devices = max_devices;

    int rc = xpdk_msg_send_sync(msg);
    if (rc == XPDK_SUCCESS) {
        rc = msg->list.count;
    }

    xpdk_msg_free(msg);
    return rc;
}

xpdk_fd_t
xpdk_open(const char *bdev_name, int flags)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (bdev_name == NULL) {
        return XPDK_ERROR_INVALID;
    }

    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_OPEN);
    if (msg == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    msg->open.bdev_name = bdev_name;
    msg->open.flags = flags;

    int rc = xpdk_msg_send_sync(msg);
    xpdk_fd_t fd = (rc == XPDK_SUCCESS) ? msg->open.result_fd : rc;

    xpdk_msg_free(msg);
    return fd;
}

int
xpdk_close(xpdk_fd_t fd)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_CLOSE);
    if (msg == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    msg->close.fd = fd;

    int rc = xpdk_msg_send_sync(msg);

    xpdk_msg_free(msg);
    return rc;
}

int
xpdk_get_info(xpdk_fd_t fd, struct xpdk_bdev_info *info)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (info == NULL) {
        return XPDK_ERROR_INVALID;
    }

    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_GET_INFO);
    if (msg == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    msg->get_info.fd = fd;
    msg->get_info.info = info;

    int rc = xpdk_msg_send_sync(msg);

    xpdk_msg_free(msg);
    return rc;
}
