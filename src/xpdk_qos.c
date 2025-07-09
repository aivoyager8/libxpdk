#include "xpdk_internal.h"
#include <string.h>

/* QoS rate limit type names (matching SPDK) */
static const char *qos_rate_limit_names[] = {
    "rw_iops",
    "rw_bps", 
    "r_bps",
    "w_bps"
};

/* Get QoS rate limit type name */
const char *xpdk_qos_get_rate_limit_name(xpdk_qos_rate_limit_type_t type)
{
    if (type >= XPDK_QOS_NUM_RATE_LIMIT_TYPES) {
        return "unknown";
    }
    return qos_rate_limit_names[type];
}

/* Set QoS rate limits for a device */
int xpdk_qos_set_rate_limits(xpdk_fd_t fd, const uint64_t *limits)
{
    struct xpdk_msg *msg;
    
    if (!limits) {
        return XPDK_ERROR_INVALID;
    }
    
    msg = xpdk_msg_alloc(XPDK_MSG_QOS_SET_LIMITS);
    if (!msg) {
        return XPDK_ERROR_NOMEM;
    }
    
    msg->qos_limits.fd = fd;
    msg->qos_limits.limits = (uint64_t *)limits;
    
    int rc = xpdk_msg_send_sync(msg);
    xpdk_msg_free(msg);
    return rc;
}

/* Get QoS rate limits for a device */
int xpdk_qos_get_rate_limits(xpdk_fd_t fd, uint64_t *limits)
{
    struct xpdk_msg *msg;
    
    if (!limits) {
        return XPDK_ERROR_INVALID;
    }
    
    msg = xpdk_msg_alloc(XPDK_MSG_QOS_GET_LIMITS);
    if (!msg) {
        return XPDK_ERROR_NOMEM;
    }
    
    msg->qos_limits.fd = fd;
    msg->qos_limits.limits = limits;
    
    int rc = xpdk_msg_send_sync(msg);
    xpdk_msg_free(msg);
    return rc;
}

/* Enable QoS with simple IOPS and bandwidth limits */
int xpdk_qos_enable_simple(xpdk_fd_t fd, uint64_t rw_iops_limit, uint64_t rw_bps_limit)
{
    uint64_t limits[XPDK_QOS_NUM_RATE_LIMIT_TYPES] = {0};
    
    limits[XPDK_QOS_RW_IOPS_RATE_LIMIT] = rw_iops_limit;
    limits[XPDK_QOS_RW_BPS_RATE_LIMIT] = rw_bps_limit;
    
    return xpdk_qos_set_rate_limits(fd, limits);
}

/* Disable all QoS limits for a device */
int xpdk_qos_disable(xpdk_fd_t fd)
{
    uint64_t limits[XPDK_QOS_NUM_RATE_LIMIT_TYPES] = {0};
    
    return xpdk_qos_set_rate_limits(fd, limits);
}

/* QoS set limits completion callback */
static void xpdk_qos_set_limits_complete(void *cb_arg, int status)
{
    struct xpdk_msg *msg = (struct xpdk_msg *)cb_arg;
    msg->status = (status == 0) ? XPDK_SUCCESS : XPDK_ERROR_QOS;
    msg->completed = true;
}

/* SPDK thread handler for QoS set limits */
void xpdk_spdk_handle_qos_set_limits(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->qos_limits.fd);
    
    if (!dev) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }
    
    /* Use SPDK's native QoS API */
    spdk_bdev_set_qos_rate_limits(dev->bdev, msg->qos_limits.limits, 
                                  xpdk_qos_set_limits_complete, msg);
}

/* SPDK thread handler for QoS get limits */
void xpdk_spdk_handle_qos_get_limits(struct xpdk_msg *msg)
{
    struct xpdk_device *dev = xpdk_get_device(msg->qos_limits.fd);
    
    if (!dev) {
        msg->status = XPDK_ERROR_INVALID;
        msg->completed = true;
        return;
    }
    
    /* Use SPDK's native QoS API */
    spdk_bdev_get_qos_rate_limits(dev->bdev, msg->qos_limits.limits);
    
    msg->status = XPDK_SUCCESS;
    msg->completed = true;
}
