#include "xpdk_internal.h"
#include "xpdk_advanced.h"
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <spdk/bdev.h>
#include <spdk/thread.h>

/* Internal structures for advanced open management */
struct xpdk_device_group_internal {
    uint32_t group_id;
    uint32_t device_count;
    char group_name[256];
    xpdk_fd_t devices[XPDK_MAX_BATCH_DEVICES];
    uint64_t total_capacity;
    uint32_t total_bandwidth_mbps;
    bool redundancy_enabled;
    bool in_use;
};

struct xpdk_custom_device_registry {
    uint32_t count;
    struct xpdk_custom_device_type types[32]; /* Maximum 32 custom device types */
};

/* Global advanced open context */
struct xpdk_advanced_context {
    struct xpdk_device_group_internal groups[64]; /* Maximum 64 device groups */
    uint32_t next_group_id;
    struct xpdk_custom_device_registry custom_registry;
    pthread_mutex_t groups_lock;
    pthread_mutex_t registry_lock;
    bool initialized;
};

static struct xpdk_advanced_context g_advanced_ctx = {0};

/* Initialize advanced open context */
static int
xpdk_advanced_init_context(void)
{
    if (g_advanced_ctx.initialized) {
        return XPDK_SUCCESS;
    }

    memset(&g_advanced_ctx, 0, sizeof(g_advanced_ctx));
    g_advanced_ctx.next_group_id = 1;

    int rc = pthread_mutex_init(&g_advanced_ctx.groups_lock, NULL);
    if (rc != 0) {
        return XPDK_ERROR_NOMEM;
    }

    rc = pthread_mutex_init(&g_advanced_ctx.registry_lock, NULL);
    if (rc != 0) {
        pthread_mutex_destroy(&g_advanced_ctx.groups_lock);
        return XPDK_ERROR_NOMEM;
    }

    g_advanced_ctx.initialized = true;
    return XPDK_SUCCESS;
}

/* Cleanup advanced open context */
static void
xpdk_advanced_cleanup_context(void)
{
    if (!g_advanced_ctx.initialized) {
        return;
    }

    pthread_mutex_destroy(&g_advanced_ctx.groups_lock);
    pthread_mutex_destroy(&g_advanced_ctx.registry_lock);
    g_advanced_ctx.initialized = false;
}

/* Get current timestamp in microseconds */
static uint64_t
xpdk_get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
}

/* Initialize open options with defaults */
void
xpdk_open_opts_init(struct xpdk_open_opts *opts)
{
    if (opts == NULL) {
        return;
    }

    memset(opts, 0, sizeof(*opts));
    opts->flags = O_RDWR;
    opts->timeout_ms = 30000; /* 30 second default timeout */
    opts->allow_partial_failure = false;
    opts->enable_turbo = false;
    opts->cpu_core = -1;
    opts->queue_depth = 0; /* Use default */
    opts->io_cache_size = 0; /* No cache by default */
    opts->qos_limits = NULL;
    opts->qos_enable_stats = false;
    opts->custom_data = NULL;
    opts->custom_data_size = 0;
    opts->enable_auto_retry = true;
    opts->max_retry_count = 3;
    opts->retry_delay_ms = 100;
}

/* Initialize discovery options with defaults */
void
xpdk_discovery_opts_init(struct xpdk_discovery_opts *opts)
{
    if (opts == NULL) {
        return;
    }

    memset(opts, 0, sizeof(*opts));
    opts->include_system_devices = true;
    opts->include_nvme_devices = true;
    opts->include_custom_devices = true;
    opts->deep_scan = false;
    opts->timeout_ms = 10000; /* 10 second timeout */
    opts->filter_pattern = NULL;
}

/* Validate device name */
static int
xpdk_validate_device_name(const char *bdev_name)
{
    if (bdev_name == NULL || strlen(bdev_name) == 0) {
        return XPDK_ERROR_INVALID;
    }

    if (strlen(bdev_name) >= 256) {
        return XPDK_ERROR_INVALID;
    }

    return XPDK_SUCCESS;
}

/* Validate open options */
static int
xpdk_validate_open_opts(const struct xpdk_open_opts *opts)
{
    if (opts == NULL) {
        return XPDK_ERROR_INVALID;
    }

    if (opts->flags & ~(O_RDONLY | O_WRONLY | O_RDWR)) {
        return XPDK_ERROR_INVALID;
    }

    if (opts->timeout_ms > 300000) { /* Max 5 minutes */
        return XPDK_ERROR_INVALID;
    }

    if (opts->cpu_core < -1 || opts->cpu_core >= 1024) {
        return XPDK_ERROR_INVALID;
    }

    if (opts->queue_depth > 32768) {
        return XPDK_ERROR_INVALID;
    }

    if (opts->io_cache_size > 1024) { /* Max 1GB cache */
        return XPDK_ERROR_INVALID;
    }

    if (opts->max_retry_count > 100) {
        return XPDK_ERROR_INVALID;
    }

    if (opts->retry_delay_ms > 10000) { /* Max 10 second delay */
        return XPDK_ERROR_INVALID;
    }

    return XPDK_SUCCESS;
}

/* Enhanced device opening with advanced options */
static int
xpdk_open_device_enhanced(const char *bdev_name, const struct xpdk_open_opts *opts,
                          struct xpdk_device_desc *desc)
{
    int rc;
    uint32_t retry_count = 0;
    uint64_t start_time = xpdk_get_timestamp_us();

    /* Validate inputs */
    rc = xpdk_validate_device_name(bdev_name);
    if (rc != XPDK_SUCCESS) {
        return rc;
    }

    rc = xpdk_validate_open_opts(opts);
    if (rc != XPDK_SUCCESS) {
        return rc;
    }

    /* Initialize device descriptor */
    memset(desc, 0, sizeof(*desc));
    strncpy(desc->name, bdev_name, sizeof(desc->name) - 1);
    desc->state = XPDK_DEVICE_STATE_OPENING;
    desc->open_timestamp = start_time;
    memcpy(&desc->opts, opts, sizeof(*opts));

    /* Attempt to open device with retry logic */
    while (retry_count <= opts->max_retry_count) {
        /* Try to open device using standard API */
        desc->fd = xpdk_open(bdev_name, opts->flags);
        
        if (desc->fd >= 0) {
            /* Success - get device info */
            rc = xpdk_get_info(desc->fd, &desc->info);
            if (rc == XPDK_SUCCESS) {
                desc->state = XPDK_DEVICE_STATE_OPEN;
                
                /* Apply advanced options */
                if (opts->qos_limits != NULL) {
                    xpdk_qos_set_rate_limits(desc->fd, opts->qos_limits->limits);
                }
                
                /* Set CPU affinity if requested */
                if (opts->cpu_core >= 0) {
                    xpdk_set_cpu_affinity(opts->cpu_core);
                }
                
                return XPDK_SUCCESS;
            } else {
                /* Failed to get info - close and retry */
                xpdk_close(desc->fd);
                desc->fd = -1;
            }
        }

        /* Check timeout */
        uint64_t elapsed = xpdk_get_timestamp_us() - start_time;
        if (elapsed >= opts->timeout_ms * 1000) {
            desc->state = XPDK_DEVICE_STATE_ERROR;
            return XPDK_ERROR_IO;
        }

        /* Retry if enabled */
        if (opts->enable_auto_retry && retry_count < opts->max_retry_count) {
            retry_count++;
            usleep(opts->retry_delay_ms * 1000);
        } else {
            break;
        }
    }

    desc->state = XPDK_DEVICE_STATE_ERROR;
    return XPDK_ERROR_IO;
}

/* Open a single device with advanced options */
int
xpdk_open_advanced(const char *bdev_name, const struct xpdk_open_opts *opts,
                   struct xpdk_device_desc *desc)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    /* Initialize advanced context if needed */
    int rc = xpdk_advanced_init_context();
    if (rc != XPDK_SUCCESS) {
        return rc;
    }

    if (desc == NULL) {
        return XPDK_ERROR_INVALID;
    }

    return xpdk_open_device_enhanced(bdev_name, opts, desc);
}

/* Resolve device dependencies */
static int
xpdk_resolve_dependencies(const struct xpdk_open_request *requests, uint32_t count,
                          uint32_t *open_order)
{
    /* Simple dependency resolution - topological sort */
    bool visited[XPDK_MAX_BATCH_DEVICES] = {false};
    uint32_t order_index = 0;

    /* First pass: devices with no dependencies */
    for (uint32_t i = 0; i < count; i++) {
        if (requests[i].dependency_count == 0) {
            open_order[order_index++] = i;
            visited[i] = true;
        }
    }

    /* Subsequent passes: devices whose dependencies are satisfied */
    bool progress = true;
    while (progress && order_index < count) {
        progress = false;
        
        for (uint32_t i = 0; i < count; i++) {
            if (visited[i]) {
                continue;
            }

            /* Check if all dependencies are satisfied */
            bool all_deps_satisfied = true;
            for (uint32_t j = 0; j < requests[i].dependency_count; j++) {
                bool found = false;
                for (uint32_t k = 0; k < count; k++) {
                    if (visited[k] && 
                        strcmp(requests[k].bdev_name, requests[i].dependencies[j]) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    all_deps_satisfied = false;
                    break;
                }
            }

            if (all_deps_satisfied) {
                open_order[order_index++] = i;
                visited[i] = true;
                progress = true;
            }
        }
    }

    return (order_index == count) ? XPDK_SUCCESS : XPDK_ERROR_INVALID;
}

/* Open multiple devices in a batch operation */
int
xpdk_open_batch(const struct xpdk_open_request *requests, uint32_t count,
                struct xpdk_open_result *result)
{
    if (!g_xpdk_ctx.initialized) {
        return XPDK_ERROR_INVALID;
    }

    if (requests == NULL || count == 0 || count > XPDK_MAX_BATCH_DEVICES || result == NULL) {
        return XPDK_ERROR_INVALID;
    }

    /* Initialize advanced context if needed */
    int rc = xpdk_advanced_init_context();
    if (rc != XPDK_SUCCESS) {
        return rc;
    }

    uint64_t start_time = xpdk_get_timestamp_us();

    /* Initialize result structure */
    memset(result, 0, sizeof(*result));
    result->requested_count = count;
    result->status = XPDK_SUCCESS;

    /* Resolve dependencies */
    uint32_t open_order[XPDK_MAX_BATCH_DEVICES];
    rc = xpdk_resolve_dependencies(requests, count, open_order);
    if (rc != XPDK_SUCCESS) {
        result->status = rc;
        snprintf(result->error_details, sizeof(result->error_details),
                 "Failed to resolve device dependencies");
        return rc;
    }

    /* Open devices in dependency order */
    for (uint32_t i = 0; i < count; i++) {
        uint32_t req_idx = open_order[i];
        const struct xpdk_open_request *req = &requests[req_idx];
        struct xpdk_device_desc *desc = &result->devices[result->opened_count];

        rc = xpdk_open_device_enhanced(req->bdev_name, &req->opts, desc);
        if (rc == XPDK_SUCCESS) {
            desc->group_id = req->group_id;
            
            /* Set up dependencies */
            desc->dependency_count = req->dependency_count;
            for (uint32_t j = 0; j < req->dependency_count; j++) {
                /* Find dependency file descriptor */
                for (uint32_t k = 0; k < result->opened_count; k++) {
                    if (strcmp(result->devices[k].name, req->dependencies[j]) == 0) {
                        desc->dependencies[j] = result->devices[k].fd;
                        break;
                    }
                }
            }
            
            result->opened_count++;
        } else {
            result->failed_count++;
            
            /* Handle partial failure */
            if (!req->opts.allow_partial_failure) {
                /* Close all previously opened devices */
                for (uint32_t j = 0; j < result->opened_count; j++) {
                    xpdk_close_advanced(&result->devices[j]);
                }
                result->opened_count = 0;
                result->failed_count = count;
                result->status = rc;
                snprintf(result->error_details, sizeof(result->error_details),
                         "Failed to open device '%s': %s", req->bdev_name, xpdk_strerror(rc));
                return rc;
            }
        }
    }

    result->operation_time_us = xpdk_get_timestamp_us() - start_time;
    
    /* Overall success if at least one device opened */
    if (result->opened_count > 0) {
        result->status = XPDK_SUCCESS;
    } else {
        result->status = XPDK_ERROR_IO;
        snprintf(result->error_details, sizeof(result->error_details),
                 "No devices could be opened");
    }

    return result->status;
}

/* Open devices with automatic dependency resolution */
int
xpdk_open_auto_resolve(const char **device_names, uint32_t count,
                       const struct xpdk_open_opts *opts,
                       struct xpdk_open_result *result)
{
    if (device_names == NULL || count == 0 || count > XPDK_MAX_BATCH_DEVICES || 
        opts == NULL || result == NULL) {
        return XPDK_ERROR_INVALID;
    }

    /* Convert to batch requests */
    struct xpdk_open_request *requests = calloc(count, sizeof(struct xpdk_open_request));
    if (requests == NULL) {
        return XPDK_ERROR_NOMEM;
    }

    for (uint32_t i = 0; i < count; i++) {
        strncpy(requests[i].bdev_name, device_names[i], sizeof(requests[i].bdev_name) - 1);
        memcpy(&requests[i].opts, opts, sizeof(*opts));
        requests[i].group_id = 0; /* No group by default */
        requests[i].dependency_count = 0; /* No dependencies for auto-resolve */
        requests[i].priority = 0;
    }

    int rc = xpdk_open_batch(requests, count, result);
    free(requests);
    
    return rc;
}

/* Close devices opened with advanced API */
int
xpdk_close_advanced(struct xpdk_device_desc *desc)
{
    if (desc == NULL || desc->fd < 0) {
        return XPDK_ERROR_INVALID;
    }

    desc->state = XPDK_DEVICE_STATE_CLOSING;
    
    int rc = xpdk_close(desc->fd);
    if (rc == XPDK_SUCCESS) {
        desc->state = XPDK_DEVICE_STATE_CLOSED;
        desc->fd = -1;
    } else {
        desc->state = XPDK_DEVICE_STATE_ERROR;
    }

    return rc;
}

/* Close multiple devices in a batch operation */
int
xpdk_close_batch(struct xpdk_device_desc *descs, uint32_t count)
{
    if (descs == NULL || count == 0) {
        return XPDK_ERROR_INVALID;
    }

    int failed_count = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        if (xpdk_close_advanced(&descs[i]) != XPDK_SUCCESS) {
            failed_count++;
        }
    }

    return (failed_count == 0) ? XPDK_SUCCESS : XPDK_ERROR_IO;
}

/* Get device state */
xpdk_device_state_t
xpdk_get_device_state(xpdk_fd_t fd)
{
    struct xpdk_device *dev = xpdk_get_device(fd);
    if (dev == NULL) {
        return XPDK_DEVICE_STATE_ERROR;
    }

    return XPDK_DEVICE_STATE_OPEN; /* Simple implementation */
}

/* Wait for device state change */
int
xpdk_wait_device_state(xpdk_fd_t fd, xpdk_device_state_t target_state, uint32_t timeout_ms)
{
    if (fd < 0) {
        return XPDK_ERROR_INVALID;
    }

    uint64_t start_time = xpdk_get_timestamp_us();
    uint64_t timeout_us = timeout_ms * 1000;

    while (true) {
        xpdk_device_state_t current_state = xpdk_get_device_state(fd);
        if (current_state == target_state) {
            return XPDK_SUCCESS;
        }

        if (current_state == XPDK_DEVICE_STATE_ERROR) {
            return XPDK_ERROR_IO;
        }

        if (timeout_ms > 0) {
            uint64_t elapsed = xpdk_get_timestamp_us() - start_time;
            if (elapsed >= timeout_us) {
                return XPDK_ERROR_IO; /* Timeout */
            }
        }

        usleep(1000); /* Sleep 1ms */
    }
}

/* Validate device dependencies */
int
xpdk_validate_dependencies(const struct xpdk_device_desc *desc)
{
    if (desc == NULL) {
        return XPDK_ERROR_INVALID;
    }

    /* Check all dependency devices are open */
    for (uint32_t i = 0; i < desc->dependency_count; i++) {
        if (xpdk_get_device_state(desc->dependencies[i]) != XPDK_DEVICE_STATE_OPEN) {
            return XPDK_ERROR_IO;
        }
    }

    return XPDK_SUCCESS;
}

/* Get detailed device statistics */
int
xpdk_get_device_stats(xpdk_fd_t fd, struct xpdk_perf_stats *stats)
{
    if (stats == NULL) {
        return XPDK_ERROR_INVALID;
    }

    /* Use existing performance statistics API */
    return xpdk_get_perf_stats(fd, stats);
}

/* Reset device statistics */
int
xpdk_reset_device_stats(xpdk_fd_t fd)
{
    /* Use existing performance statistics API */
    return xpdk_reset_perf_stats(fd);
}

/* Get device group information */
int
xpdk_get_device_group(uint32_t group_id, struct xpdk_device_group *group)
{
    if (group == NULL) {
        return XPDK_ERROR_INVALID;
    }

    pthread_mutex_lock(&g_advanced_ctx.groups_lock);
    
    /* Find the group */
    for (int i = 0; i < 64; i++) {
        if (g_advanced_ctx.groups[i].in_use && 
            g_advanced_ctx.groups[i].group_id == group_id) {
            
            /* Copy group information */
            group->group_id = g_advanced_ctx.groups[i].group_id;
            group->device_count = g_advanced_ctx.groups[i].device_count;
            strncpy(group->group_name, g_advanced_ctx.groups[i].group_name, 
                   sizeof(group->group_name) - 1);
            group->group_name[sizeof(group->group_name) - 1] = '\0';
            
            /* Copy device list */
            for (uint32_t j = 0; j < g_advanced_ctx.groups[i].device_count; j++) {
                group->devices[j] = g_advanced_ctx.groups[i].devices[j];
            }
            
            group->total_capacity = g_advanced_ctx.groups[i].total_capacity;
            group->total_bandwidth_mbps = g_advanced_ctx.groups[i].total_bandwidth_mbps;
            group->redundancy_enabled = g_advanced_ctx.groups[i].redundancy_enabled;
            
            pthread_mutex_unlock(&g_advanced_ctx.groups_lock);
            return XPDK_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_advanced_ctx.groups_lock);
    return XPDK_ERROR_NODEV;
}

/* Create a device group */
int
xpdk_create_device_group(const char *group_name, const xpdk_fd_t *device_fds,
                         uint32_t count, uint32_t *group_id)
{
    if (device_fds == NULL || count == 0 || count > XPDK_MAX_BATCH_DEVICES || 
        group_id == NULL) {
        return XPDK_ERROR_INVALID;
    }

    /* Initialize advanced context if needed */
    int rc = xpdk_advanced_init_context();
    if (rc != XPDK_SUCCESS) {
        return rc;
    }

    pthread_mutex_lock(&g_advanced_ctx.groups_lock);
    
    /* Find free group slot */
    int group_idx = -1;
    for (int i = 0; i < 64; i++) {
        if (!g_advanced_ctx.groups[i].in_use) {
            group_idx = i;
            break;
        }
    }
    
    if (group_idx == -1) {
        pthread_mutex_unlock(&g_advanced_ctx.groups_lock);
        return XPDK_ERROR_NOMEM;
    }

    struct xpdk_device_group_internal *group = &g_advanced_ctx.groups[group_idx];
    
    /* Initialize group */
    memset(group, 0, sizeof(*group));
    group->group_id = g_advanced_ctx.next_group_id++;
    group->device_count = count;
    group->in_use = true;
    
    if (group_name != NULL) {
        strncpy(group->group_name, group_name, sizeof(group->group_name) - 1);
        group->group_name[sizeof(group->group_name) - 1] = '\0';
    }
    
    /* Copy device file descriptors and calculate totals */
    uint64_t total_capacity = 0;
    for (uint32_t i = 0; i < count; i++) {
        group->devices[i] = device_fds[i];
        
        /* Get device info for capacity calculation */
        struct xpdk_bdev_info info;
        if (xpdk_get_info(device_fds[i], &info) == XPDK_SUCCESS) {
            total_capacity += info.capacity;
        }
    }
    
    group->total_capacity = total_capacity;
    group->total_bandwidth_mbps = count * 1000; /* Estimate: 1GB/s per device */
    group->redundancy_enabled = false; /* Default: no redundancy */
    
    *group_id = group->group_id;
    
    pthread_mutex_unlock(&g_advanced_ctx.groups_lock);
    return XPDK_SUCCESS;
}

/* Destroy a device group */
int
xpdk_destroy_device_group(uint32_t group_id)
{
    pthread_mutex_lock(&g_advanced_ctx.groups_lock);
    
    /* Find and destroy the group */
    for (int i = 0; i < 64; i++) {
        if (g_advanced_ctx.groups[i].in_use && 
            g_advanced_ctx.groups[i].group_id == group_id) {
            
            memset(&g_advanced_ctx.groups[i], 0, sizeof(g_advanced_ctx.groups[i]));
            pthread_mutex_unlock(&g_advanced_ctx.groups_lock);
            return XPDK_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_advanced_ctx.groups_lock);
    return XPDK_ERROR_NODEV;
}

/* Register a custom device type */
int
xpdk_register_custom_device_type(const struct xpdk_custom_device_type *device_type)
{
    if (device_type == NULL || device_type->name[0] == '\0' || 
        device_type->create_fn == NULL) {
        return XPDK_ERROR_INVALID;
    }

    /* Initialize advanced context if needed */
    int rc = xpdk_advanced_init_context();
    if (rc != XPDK_SUCCESS) {
        return rc;
    }

    pthread_mutex_lock(&g_advanced_ctx.registry_lock);
    
    /* Check if type already exists */
    for (uint32_t i = 0; i < g_advanced_ctx.custom_registry.count; i++) {
        if (strcmp(g_advanced_ctx.custom_registry.types[i].name, device_type->name) == 0) {
            pthread_mutex_unlock(&g_advanced_ctx.registry_lock);
            return XPDK_ERROR_BUSY; /* Type already registered */
        }
    }
    
    /* Check if we have space */
    if (g_advanced_ctx.custom_registry.count >= 32) {
        pthread_mutex_unlock(&g_advanced_ctx.registry_lock);
        return XPDK_ERROR_NOMEM;
    }
    
    /* Add new type */
    uint32_t idx = g_advanced_ctx.custom_registry.count++;
    memcpy(&g_advanced_ctx.custom_registry.types[idx], device_type, 
           sizeof(*device_type));
    
    pthread_mutex_unlock(&g_advanced_ctx.registry_lock);
    return XPDK_SUCCESS;
}

/* Unregister a custom device type */
int
xpdk_unregister_custom_device_type(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return XPDK_ERROR_INVALID;
    }

    pthread_mutex_lock(&g_advanced_ctx.registry_lock);
    
    /* Find and remove the type */
    for (uint32_t i = 0; i < g_advanced_ctx.custom_registry.count; i++) {
        if (strcmp(g_advanced_ctx.custom_registry.types[i].name, name) == 0) {
            /* Move remaining types down */
            for (uint32_t j = i; j < g_advanced_ctx.custom_registry.count - 1; j++) {
                memcpy(&g_advanced_ctx.custom_registry.types[j],
                       &g_advanced_ctx.custom_registry.types[j + 1],
                       sizeof(g_advanced_ctx.custom_registry.types[j]));
            }
            g_advanced_ctx.custom_registry.count--;
            
            pthread_mutex_unlock(&g_advanced_ctx.registry_lock);
            return XPDK_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&g_advanced_ctx.registry_lock);
    return XPDK_ERROR_NODEV;
}

/* Create and open a custom device */
int
xpdk_create_custom_device(const char *device_type, const char *config,
                          const struct xpdk_open_opts *opts,
                          struct xpdk_device_desc *desc)
{
    if (device_type == NULL || config == NULL || opts == NULL || desc == NULL) {
        return XPDK_ERROR_INVALID;
    }

    pthread_mutex_lock(&g_advanced_ctx.registry_lock);
    
    /* Find the device type */
    struct xpdk_custom_device_type *type = NULL;
    for (uint32_t i = 0; i < g_advanced_ctx.custom_registry.count; i++) {
        if (strcmp(g_advanced_ctx.custom_registry.types[i].name, device_type) == 0) {
            type = &g_advanced_ctx.custom_registry.types[i];
            break;
        }
    }
    
    if (type == NULL) {
        pthread_mutex_unlock(&g_advanced_ctx.registry_lock);
        return XPDK_ERROR_NODEV;
    }
    
    /* Call the creation function */
    int rc = type->create_fn(config, opts, desc);
    
    pthread_mutex_unlock(&g_advanced_ctx.registry_lock);
    return rc;
}

/* Discover available devices */
int
xpdk_discover_devices(const struct xpdk_discovery_opts *opts,
                      struct xpdk_discovery_result *result)
{
    if (opts == NULL || result == NULL) {
        return XPDK_ERROR_INVALID;
    }

    uint64_t start_time = xpdk_get_timestamp_us();
    
    /* Initialize result */
    memset(result, 0, sizeof(*result));
    
    /* Use existing xpdk_list_bdevs for basic discovery */
    int count = xpdk_list_bdevs(result->devices, 256);
    if (count < 0) {
        return count;
    }
    
    result->device_count = count;
    result->discovery_time_us = xpdk_get_timestamp_us() - start_time;
    
    /* Add basic topology information */
    snprintf(result->topology_info, sizeof(result->topology_info),
             "Discovered %u devices in %lu microseconds", 
             result->device_count, result->discovery_time_us);
    
    return XPDK_SUCCESS;
}

/* Cleanup function to be called during xpdk_cleanup */
void
xpdk_advanced_cleanup(void)
{
    xpdk_advanced_cleanup_context();
}
