#ifndef XPDK_H
#define XPDK_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h> // For pthread_mutex_t, pthread_cond_t

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
#define XPDK_SUCCESS         0
#define XPDK_ERROR_INVALID  -1
#define XPDK_ERROR_NOMEM    -2
#define XPDK_ERROR_IO       -/**
 * Submit a batch of I/O operations
 * @param ctx Batch context
 * @param ios Array of batch I/O operations
 * @param count Number of operations in array
 * @param callback Callback function called for each completed operation
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_batch_submit(struct xpdk_batch_ctx *ctx, struct xpdk_batch_io *ios, int count, xpdk_io_callback_t callback);ne XPDK_ERROR_BUSY     -4
#define XPDK_ERROR_NODEV    -5
#define XPDK_ERROR_QOS      -6

/* File descriptor type */
typedef int xpdk_fd_t;

/* Block device information */
struct xpdk_bdev_info {
    char name[256];           /* Device name */
    uint64_t block_size;      /* Block size in bytes */
    uint64_t num_blocks;      /* Total number of blocks */
    uint64_t capacity;        /* Total capacity in bytes */
};

/* I/O operation types */
typedef enum {
    XPDK_IO_READ,
    XPDK_IO_WRITE,
    XPDK_IO_FLUSH
} xpdk_io_type_t;

/* QoS rate limit types (matching SPDK bdev QoS types) */
typedef enum {
    XPDK_QOS_RW_IOPS_RATE_LIMIT = 0,    /* IOPS rate limit for both read and write */
    XPDK_QOS_RW_BPS_RATE_LIMIT,         /* Byte per second rate limit for both read and write */
    XPDK_QOS_R_BPS_RATE_LIMIT,          /* Byte per second rate limit for read only */
    XPDK_QOS_W_BPS_RATE_LIMIT,          /* Byte per second rate limit for write only */
    XPDK_QOS_NUM_RATE_LIMIT_TYPES       /* Keep last */
} xpdk_qos_rate_limit_type_t;

/* QoS configuration structure */
struct xpdk_qos_limits {
    uint64_t limits[XPDK_QOS_NUM_RATE_LIMIT_TYPES];  /* QoS rate limits array */
};

/* I/O vector structure for vectored I/O operations */
struct xpdk_iovec {
    void *iov_base;    /* Starting address of buffer */
    size_t iov_len;    /* Length of buffer */
};

/* Performance statistics structure */
struct xpdk_perf_stats {
    uint64_t total_read_ops;        /* Total read operations */
    uint64_t total_write_ops;       /* Total write operations */
    uint64_t total_bytes_read;      /* Total bytes read */
    uint64_t total_bytes_written;   /* Total bytes written */
    uint64_t avg_read_latency_us;   /* Average read latency in microseconds */
    uint64_t avg_write_latency_us;  /* Average write latency in microseconds */
    uint64_t current_iops;          /* Current IOPS */
    uint64_t current_bandwidth;     /* Current bandwidth in bytes/sec */
    uint64_t queue_depth;           /* Current queue depth */
    uint64_t errors;                /* Total error count */
};

/* Batch I/O operation structure */
struct xpdk_batch_io {
    xpdk_io_type_t type;    /* I/O operation type */
    xpdk_fd_t fd;           /* File descriptor */
    void *buffer;           /* Buffer for I/O */
    size_t count;           /* Number of bytes */
    uint64_t offset;        /* Offset in device */
    void *user_ctx;         /* User context */
    int status;             /* Operation status (for completion) */
    size_t bytes_transferred; /* Bytes actually transferred */
};

/* Batch I/O context structure */
struct xpdk_batch_ctx {
    int max_ios;            /* Maximum I/O operations in flight */
    int pending_ios;        /* Current pending I/O operations */
    int completed_ios;      /* Completed I/O operations ready for processing */
    struct xpdk_batch_io *pending_queue;    /* Queue of pending I/O operations */
    struct xpdk_batch_io *completed_queue;  /* Queue of completed I/O operations */
    xpdk_io_callback_t global_callback;     /* Global completion callback */
    pthread_mutex_t lock;   /* Thread safety lock */
    pthread_cond_t completion_cond;  /* Completion condition variable */
};

/* Async I/O callback function */
typedef void (*xpdk_io_callback_t)(void *ctx, int status);

/* Async I/O context */
struct xpdk_io_ctx {
    xpdk_io_callback_t callback;
    void *user_data;
    xpdk_io_type_t type;
    uint64_t offset;
    size_t length;
    void *buffer;
};

/* XPDK initialization options */
struct xpdk_opts {
    const char *config_file;        /* SPDK configuration file path */
    bool turbo_mode;                /* Enable turbo mode (high-performance polling) */
    int cpu_core;                   /* CPU core to bind SPDK thread (-1 for no binding) */
    uint32_t msg_ring_size;         /* Message ring size (0 for default) */
    uint32_t msg_pool_size;         /* Message pool size (0 for default) */
    uint32_t poll_period_us;        /* Polling period in microseconds (0 for busy polling) */
};

/**
 * Initialize XPDK options structure with defaults
 * @param opts Options structure to initialize
 */
void xpdk_opts_init(struct xpdk_opts *opts);

/**
 * Initialize the XPDK library
 * @param config_file Path to SPDK configuration file (can be NULL for default)
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_init(const char *config_file);

/**
 * Initialize the XPDK library with advanced options
 * @param opts Initialization options structure
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_init_opts(const struct xpdk_opts *opts);

/**
 * Cleanup and shutdown the XPDK library
 */
void xpdk_cleanup(void);

/**
 * List available block devices
 * @param devices Array to store device information
 * @param max_devices Maximum number of devices to return
 * @return Number of devices found, or negative error code on failure
 */
int xpdk_list_bdevs(struct xpdk_bdev_info *devices, int max_devices);

/**
 * Open a block device (similar to open())
 * @param bdev_name Name of the block device
 * @param flags Open flags (O_RDONLY, O_WRONLY, O_RDWR)
 * @return File descriptor on success, negative error code on failure
 */
xpdk_fd_t xpdk_open(const char *bdev_name, int flags);

/**
 * Close a block device (similar to close())
 * @param fd File descriptor returned by xpdk_open()
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_close(xpdk_fd_t fd);

/**
 * Read data from block device (similar to pread())
 * @param fd File descriptor
 * @param buffer Buffer to store read data
 * @param count Number of bytes to read
 * @param offset Offset in bytes from beginning of device
 * @return Number of bytes read on success, negative error code on failure
 */
ssize_t xpdk_read(xpdk_fd_t fd, void *buffer, size_t count, uint64_t offset);

/**
 * Write data to block device (similar to pwrite())
 * @param fd File descriptor
 * @param buffer Buffer containing data to write
 * @param count Number of bytes to write
 * @param offset Offset in bytes from beginning of device
 * @return Number of bytes written on success, negative error code on failure
 */
ssize_t xpdk_write(xpdk_fd_t fd, const void *buffer, size_t count, uint64_t offset);

/**
 * Flush pending writes to device (similar to fsync())
 * @param fd File descriptor
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_flush(xpdk_fd_t fd);

/**
 * Get device information
 * @param fd File descriptor
 * @param info Pointer to structure to store device information
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_get_info(xpdk_fd_t fd, struct xpdk_bdev_info *info);

/**
 * Asynchronous read operation
 * @param fd File descriptor
 * @param buffer Buffer to store read data
 * @param count Number of bytes to read
 * @param offset Offset in bytes from beginning of device
 * @param callback Callback function to call when operation completes
 * @param ctx User context passed to callback
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_read_async(xpdk_fd_t fd, void *buffer, size_t count, uint64_t offset,
                    xpdk_io_callback_t callback, void *ctx);

/**
 * Asynchronous write operation
 * @param fd File descriptor
 * @param buffer Buffer containing data to write
 * @param count Number of bytes to write
 * @param offset Offset in bytes from beginning of device
 * @param callback Callback function to call when operation completes
 * @param ctx User context passed to callback
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_write_async(xpdk_fd_t fd, const void *buffer, size_t count, uint64_t offset,
                     xpdk_io_callback_t callback, void *ctx);

/**
 * Process pending I/O operations
 * @param max_completions Maximum number of completions to process
 * @return Number of completions processed, or negative error code on failure
 */
int xpdk_poll(int max_completions);

/**
 * Get error string for error code
 * @param error_code Error code returned by other functions
 * @return Human-readable error string
 */
const char *xpdk_strerror(int error_code);

/**
 * Set QoS rate limits for a device (using SPDK native QoS)
 * @param fd File descriptor
 * @param limits QoS rate limits array (ordered by xpdk_qos_rate_limit_type_t)
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_qos_set_rate_limits(xpdk_fd_t fd, const uint64_t *limits);

/**
 * Get QoS rate limits for a device (using SPDK native QoS)
 * @param fd File descriptor
 * @param limits Pointer to array to store QoS rate limits
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_qos_get_rate_limits(xpdk_fd_t fd, uint64_t *limits);

/**
 * Get QoS rate limit type name
 * @param type QoS rate limit type
 * @return String name of the QoS rate limit type
 */
const char *xpdk_qos_get_rate_limit_name(xpdk_qos_rate_limit_type_t type);

/**
 * Enable QoS for a device with simple IOPS and bandwidth limits
 * @param fd File descriptor
 * @param rw_iops_limit Read/write IOPS limit (0 = unlimited)
 * @param rw_bps_limit Read/write bandwidth limit in bytes/sec (0 = unlimited)
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_qos_enable_simple(xpdk_fd_t fd, uint64_t rw_iops_limit, uint64_t rw_bps_limit);

/**
 * Disable all QoS limits for a device
 * @param fd File descriptor
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_qos_disable(xpdk_fd_t fd);

/**
 * Vectored read operation (similar to readv())
 * @param fd File descriptor
 * @param iov Array of I/O vectors
 * @param iovcnt Number of vectors in array
 * @param offset Offset in bytes from beginning of device
 * @return Number of bytes read on success, negative error code on failure
 */
ssize_t xpdk_readv(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset);

/**
 * Vectored write operation (similar to writev())
 * @param fd File descriptor
 * @param iov Array of I/O vectors
 * @param iovcnt Number of vectors in array
 * @param offset Offset in bytes from beginning of device
 * @return Number of bytes written on success, negative error code on failure
 */
ssize_t xpdk_writev(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset);

/**
 * Asynchronous vectored read operation
 * @param fd File descriptor
 * @param iov Array of I/O vectors
 * @param iovcnt Number of vectors in array
 * @param offset Offset in bytes from beginning of device
 * @param callback Callback function to call when operation completes
 * @param ctx User context passed to callback
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_readv_async(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset,
                     xpdk_io_callback_t callback, void *ctx);

/**
 * Asynchronous vectored write operation
 * @param fd File descriptor
 * @param iov Array of I/O vectors
 * @param iovcnt Number of vectors in array
 * @param offset Offset in bytes from beginning of device
 * @param callback Callback function to call when operation completes
 * @param ctx User context passed to callback
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_writev_async(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset,
                      xpdk_io_callback_t callback, void *ctx);

/**
 * Submit batch I/O operations
 * @param ios Array of batch I/O operations
 * @param count Number of operations in array
 * @param callback Callback function called for each completed operation
 * @return XPDK_SUCCESS on success, negative error code on failure
 */
int xpdk_batch_submit(struct xpdk_batch_io *ios, int count, xpdk_io_callback_t callback);

/**
 * Wait for batch I/O completions
 * @param max_completions Maximum number of completions to wait for
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return Number of completions processed, negative error code on failure
 */
int xpdk_batch_wait(int max_completions, int timeout_ms);

/**
 * Initialize a batch I/O context
 * @param max_ios Maximum number of I/O operations in flight
 * @return Batch context pointer on success, NULL on failure
 */
struct xpdk_batch_ctx *xpdk_batch_init(int max_ios);

/**
 * Cleanup a batch I/O context
 * @param ctx Batch context to cleanup
 */
void xpdk_batch_cleanup(struct xpdk_batch_ctx *ctx);

/**
 * Submit a single I/O to batch context (non-blocking)
 * @param ctx Batch context
 * @param io I/O operation to submit
 * @return XPDK_SUCCESS on success, XPDK_ERROR_BUSY if queue full, negative error on failure
 */
int xpdk_batch_submit_one(struct xpdk_batch_ctx *ctx, const struct xpdk_batch_io *io);

/**
 * Process completions from batch context
 * @param ctx Batch context
 * @param max_completions Maximum completions to process
 * @return Number of completions processed, negative error on failure
 */
int xpdk_batch_process_completions(struct xpdk_batch_ctx *ctx, int max_completions);



#ifdef __cplusplus
}
#endif

#endif /* XPDK_H */
