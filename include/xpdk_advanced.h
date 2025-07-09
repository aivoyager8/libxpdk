#ifndef XPDK_ADVANCED_H
#define XPDK_ADVANCED_H

#include "xpdk.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of devices in a batch operation */
#define XPDK_MAX_BATCH_DEVICES      64

/* Maximum number of device dependencies */
#define XPDK_MAX_DEVICE_DEPENDENCIES 16

/* Maximum length of custom device configuration */
#define XPDK_MAX_CUSTOM_CONFIG_LEN   2048

/* Advanced device opening options */
struct xpdk_open_opts {
    /* Basic options */
    int flags;                          /* Open flags (O_RDONLY, O_WRONLY, O_RDWR) */
    uint32_t timeout_ms;                /* Operation timeout in milliseconds (0 = default) */
    bool allow_partial_failure;        /* Allow partial success in batch operations */
    
    /* Performance options */
    bool enable_turbo;                  /* Enable turbo mode for this device */
    int cpu_core;                       /* CPU core affinity (-1 for default) */
    uint32_t queue_depth;               /* I/O queue depth (0 = default) */
    uint32_t io_cache_size;             /* I/O cache size in MB (0 = no cache) */
    
    /* QoS options */
    struct xpdk_qos_limits *qos_limits; /* QoS limits (NULL for no limits) */
    bool qos_enable_stats;              /* Enable detailed QoS statistics */
    
    /* Custom device options */
    char custom_config[XPDK_MAX_CUSTOM_CONFIG_LEN]; /* Custom device configuration */
    void *custom_data;                  /* Custom initialization data */
    size_t custom_data_size;            /* Size of custom data */
    
    /* Error handling options */
    bool enable_auto_retry;             /* Enable automatic retry on errors */
    uint32_t max_retry_count;           /* Maximum retry attempts */
    uint32_t retry_delay_ms;            /* Delay between retries in milliseconds */
};

/* Device state enumeration */
typedef enum {
    XPDK_DEVICE_STATE_CLOSED = 0,      /* Device is closed */
    XPDK_DEVICE_STATE_OPENING,         /* Device is being opened */
    XPDK_DEVICE_STATE_OPEN,            /* Device is open and ready */
    XPDK_DEVICE_STATE_ERROR,           /* Device is in error state */
    XPDK_DEVICE_STATE_CLOSING          /* Device is being closed */
} xpdk_device_state_t;

/* Device descriptor with extended information */
struct xpdk_device_desc {
    xpdk_fd_t fd;                       /* File descriptor */
    char name[256];                     /* Device name */
    struct xpdk_bdev_info info;         /* Device information */
    xpdk_device_state_t state;          /* Device state */
    uint32_t group_id;                  /* Group identifier */
    uint32_t dependency_count;          /* Number of dependencies */
    xpdk_fd_t dependencies[XPDK_MAX_DEVICE_DEPENDENCIES]; /* Dependency file descriptors */
    void *private_data;                 /* Private data for custom devices */
    size_t private_data_size;           /* Size of private data */
    uint64_t open_timestamp;            /* Timestamp when device was opened */
    struct xpdk_open_opts opts;         /* Options used to open the device */
};

/* Batch open request structure */
struct xpdk_open_request {
    char bdev_name[256];                /* Device name to open */
    struct xpdk_open_opts opts;         /* Open options */
    uint32_t group_id;                  /* Group identifier (0 = no group) */
    uint32_t dependency_count;          /* Number of dependencies */
    char dependencies[XPDK_MAX_DEVICE_DEPENDENCIES][256]; /* Dependency device names */
    uint32_t priority;                  /* Opening priority (higher = first) */
};

/* Batch open result structure */
struct xpdk_open_result {
    int status;                         /* Overall operation status */
    uint32_t requested_count;           /* Number of devices requested */
    uint32_t opened_count;              /* Number of successfully opened devices */
    uint32_t failed_count;              /* Number of failed devices */
    struct xpdk_device_desc devices[XPDK_MAX_BATCH_DEVICES]; /* Opened device descriptors */
    char error_details[1024];           /* Detailed error information */
    uint64_t operation_time_us;         /* Total operation time in microseconds */
};

/* Device group information */
struct xpdk_device_group {
    uint32_t group_id;                  /* Group identifier */
    uint32_t device_count;              /* Number of devices in group */
    char group_name[256];               /* Group name (optional) */
    xpdk_fd_t devices[XPDK_MAX_BATCH_DEVICES]; /* Device file descriptors */
    uint64_t total_capacity;            /* Total group capacity in bytes */
    uint32_t total_bandwidth_mbps;      /* Total group bandwidth in MB/s */
    bool redundancy_enabled;            /* Whether group has redundancy */
};

/* Custom device type registration */
struct xpdk_custom_device_type {
    char name[64];                      /* Device type name */
    int (*create_fn)(const char *config, const struct xpdk_open_opts *opts,
                     struct xpdk_device_desc *desc); /* Creation function */
    int (*destroy_fn)(struct xpdk_device_desc *desc); /* Destruction function */
    void *plugin_data;                  /* Plugin-specific data */
};

/* Device discovery and enumeration */
struct xpdk_discovery_opts {
    bool include_system_devices;        /* Include system block devices */
    bool include_nvme_devices;          /* Include NVMe devices */
    bool include_custom_devices;        /* Include custom/virtual devices */
    bool deep_scan;                     /* Perform deep device scanning */
    uint32_t timeout_ms;                /* Discovery timeout */
    const char *filter_pattern;        /* Device name filter pattern */
};

/* Device discovery result */
struct xpdk_discovery_result {
    uint32_t device_count;              /* Number of discovered devices */
    struct xpdk_bdev_info devices[256]; /* Discovered devices */
    char topology_info[2048];           /* Device topology information */
    uint64_t discovery_time_us;         /* Discovery time in microseconds */
};

/**
 * Initialize open options with defaults
 * @param opts Options structure to initialize
 */
void xpdk_open_opts_init(struct xpdk_open_opts *opts);

/**
 * Open a single device with advanced options
 * @param bdev_name Name of the block device
 * @param opts Advanced open options
 * @param desc Pointer to store device descriptor
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_open_advanced(const char *bdev_name, const struct xpdk_open_opts *opts,
                       struct xpdk_device_desc *desc);

/**
 * Open multiple devices in a batch operation
 * @param requests Array of open requests
 * @param count Number of requests
 * @param result Pointer to store batch operation result
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_open_batch(const struct xpdk_open_request *requests, uint32_t count,
                    struct xpdk_open_result *result);

/**
 * Open devices with automatic dependency resolution
 * @param device_names Array of device names to open
 * @param count Number of devices
 * @param opts Common open options for all devices
 * @param result Pointer to store batch operation result
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_open_auto_resolve(const char **device_names, uint32_t count,
                           const struct xpdk_open_opts *opts,
                           struct xpdk_open_result *result);

/**
 * Create and open a custom device
 * @param device_type Type of custom device to create
 * @param config Configuration string for the device
 * @param opts Open options
 * @param desc Pointer to store device descriptor
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_create_custom_device(const char *device_type, const char *config,
                              const struct xpdk_open_opts *opts,
                              struct xpdk_device_desc *desc);

/**
 * Close devices opened with advanced API
 * @param desc Device descriptor to close
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_close_advanced(struct xpdk_device_desc *desc);

/**
 * Close multiple devices in a batch operation
 * @param descs Array of device descriptors to close
 * @param count Number of descriptors
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_close_batch(struct xpdk_device_desc *descs, uint32_t count);

/**
 * Get device group information
 * @param group_id Group identifier
 * @param group Pointer to store group information
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_get_device_group(uint32_t group_id, struct xpdk_device_group *group);

/**
 * Create a device group
 * @param group_name Group name (optional, can be NULL)
 * @param device_fds Array of device file descriptors
 * @param count Number of devices
 * @param group_id Pointer to store assigned group ID
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_create_device_group(const char *group_name, const xpdk_fd_t *device_fds,
                             uint32_t count, uint32_t *group_id);

/**
 * Destroy a device group (does not close devices)
 * @param group_id Group identifier
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_destroy_device_group(uint32_t group_id);

/**
 * Register a custom device type
 * @param device_type Custom device type information
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_register_custom_device_type(const struct xpdk_custom_device_type *device_type);

/**
 * Unregister a custom device type
 * @param name Device type name
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_unregister_custom_device_type(const char *name);

/**
 * Discover available devices
 * @param opts Discovery options
 * @param result Pointer to store discovery result
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_discover_devices(const struct xpdk_discovery_opts *opts,
                          struct xpdk_discovery_result *result);

/**
 * Get device state
 * @param fd File descriptor
 * @return Device state, or XPDK_DEVICE_STATE_ERROR on failure
 */
xpdk_device_state_t xpdk_get_device_state(xpdk_fd_t fd);

/**
 * Wait for device state change
 * @param fd File descriptor
 * @param target_state Target state to wait for
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return XPDK_SUCCESS when target state reached, negative error code on failure/timeout
 */
int xpdk_wait_device_state(xpdk_fd_t fd, xpdk_device_state_t target_state, uint32_t timeout_ms);

/**
 * Validate device dependencies
 * @param desc Device descriptor
 * @return XPDK_SUCCESS if dependencies are valid, negative error code otherwise
 */
int xpdk_validate_dependencies(const struct xpdk_device_desc *desc);

/**
 * Get detailed device statistics
 * @param fd File descriptor
 * @param stats Pointer to store statistics
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_get_device_stats(xpdk_fd_t fd, struct xpdk_perf_stats *stats);

/**
 * Reset device statistics
 * @param fd File descriptor
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_reset_device_stats(xpdk_fd_t fd);

/**
 * Initialize discovery options with defaults
 * @param opts Discovery options structure to initialize
 */
void xpdk_discovery_opts_init(struct xpdk_discovery_opts *opts);

#ifdef __cplusplus
}
#endif

#endif /* XPDK_ADVANCED_H */
