#ifndef XPDK_INTERNAL_H
#define XPDK_INTERNAL_H

#include "xpdk.h"
#include <pthread.h>
#include <spdk/env.h>
#include <spdk/bdev.h>
#include <spdk/thread.h>
#include <spdk/queue.h>
#include <spdk/event.h>

/* Maximum number of open devices */
#define XPDK_MAX_OPEN_DEVICES 256

/* Message types for SPDK thread communication */
enum xpdk_msg_type {
    XPDK_MSG_OPEN,
    XPDK_MSG_CLOSE,
    XPDK_MSG_READ,
    XPDK_MSG_WRITE,
    XPDK_MSG_READV_NATIVE,      /* Native vectored read */
    XPDK_MSG_WRITEV_NATIVE,     /* Native vectored write */
    XPDK_MSG_FLUSH,
    XPDK_MSG_LIST_BDEVS,
    XPDK_MSG_GET_INFO,
    XPDK_MSG_QOS_SET_LIMITS,
    XPDK_MSG_QOS_GET_LIMITS,
    XPDK_MSG_GET_PERF_STATS,
    XPDK_MSG_RESET_PERF_STATS,
    XPDK_MSG_TRIM,
    XPDK_MSG_WRITE_ZEROS,
    XPDK_MSG_SHUTDOWN
};

/* Message structure for SPDK thread communication */
struct xpdk_msg {
    enum xpdk_msg_type type;
    void *ctx;                      /* User context */
    
    /* Message completion */
    volatile bool completed;        /* Completion flag */
    int status;                     /* Operation status */
    
    /* Operation parameters */
    union {
        struct {
            const char *bdev_name;
            int flags;
            int result_fd;
        } open;
        
        struct {
            int fd;
        } close;
        
        struct {
            int fd;
            void *buffer;              /* For regular IO: buffer pointer; For vectored: iovec array */
            size_t count;              /* For regular IO: byte count; For vectored: iovec count */
            uint64_t offset;
            ssize_t bytes_transferred;
            xpdk_io_callback_t callback;  /* For async operations */
            void *user_ctx;               /* User callback context */
            void *spdk_ctx;               /* SPDK-specific context (e.g., converted iovec) */
        } io;
        
        struct {
            struct xpdk_bdev_info *devices;
            int max_devices;
            int count;
        } list;
        
        struct {
            int fd;
            struct xpdk_bdev_info *info;
        } get_info;
        
        struct {
            int fd;
            uint64_t *limits;      /* QoS rate limits array */
        } qos_limits;
        
        struct {
            int fd;
            struct xpdk_perf_stats *stats;
        } perf_stats;
        
        struct {
            int fd;
            uint64_t offset;
            uint64_t length;
        } trim;
    };
    
    TAILQ_ENTRY(xpdk_msg) link;
};

/* Internal device context */
struct xpdk_device {
    int fd;                          /* File descriptor */
    struct spdk_bdev *bdev;          /* SPDK bdev handle */
    struct spdk_bdev_desc *desc;     /* SPDK bdev descriptor */
    struct spdk_io_channel *channel; /* I/O channel */
    int flags;                       /* Open flags */
    bool in_use;                     /* Whether this slot is in use */
    
    /* Performance statistics */
    struct xpdk_perf_stats perf_stats;
    uint64_t stats_start_time;       /* Statistics collection start time */
    uint64_t stats_window_start;     /* Current window start time */
    uint64_t last_total_ops;         /* Last total operations count */
    uint64_t last_total_bytes;       /* Last total bytes count */
};

/* Global library state */
struct xpdk_context {
    bool initialized;                                   /* Library initialization state */
    struct xpdk_device devices[XPDK_MAX_OPEN_DEVICES]; /* Open devices array */
    
    /* Configuration options */
    struct xpdk_opts opts;                             /* Initialization options */
    
    /* SPDK thread management */
    struct spdk_thread *spdk_thread;                   /* Dedicated SPDK thread */
    pthread_t spdk_thread_id;                          /* SPDK thread ID */
    volatile bool spdk_thread_running;                 /* SPDK thread running flag */
    
    /* High-performance message queue using SPDK ring */
    struct spdk_ring *msg_ring;                        /* Lock-free message ring */
    
    /* Memory pool for messages */
    struct spdk_mempool *msg_pool;                     /* Pre-allocated message pool */
    
    /* Event poller for processing messages */
    struct spdk_poller *msg_poller;                    /* Message processing poller */
    
    /* Synchronization for blocking operations */
    pthread_mutex_t sync_lock;                         /* Lock for synchronous operations */
    pthread_cond_t sync_cond;                          /* Condition for synchronous operations */
    
    /* Performance monitoring */
    uint64_t total_messages_processed;                 /* Total messages processed */
    uint64_t total_io_operations;                      /* Total I/O operations */
};

/* Global context instance */
extern struct xpdk_context g_xpdk_ctx;

/* High-performance async IO context for vectored operations */
struct xpdk_vectored_async_ctx {
    struct xpdk_msg *msg;           /* Original message */
    struct iovec *spdk_iov;         /* Converted SPDK iovec */
    int iovcnt;                     /* Number of vectors */
    uint64_t start_time;            /* For latency measurement */
};

/* Internal helper functions */
int xpdk_find_free_fd(void);
struct xpdk_device *xpdk_get_device(xpdk_fd_t fd);
void xpdk_put_device(struct xpdk_device *dev);

/* SPDK thread management */
int xpdk_init_spdk_thread(const struct xpdk_opts *opts);
void xpdk_cleanup_spdk_thread(void);
void *xpdk_spdk_thread_main(void *arg);

/* CPU affinity management */
int xpdk_set_cpu_affinity(int cpu_core);
void xpdk_print_performance_stats(void);

/* Message handling */
struct xpdk_msg *xpdk_msg_alloc(enum xpdk_msg_type type);
void xpdk_msg_free(struct xpdk_msg *msg);
int xpdk_msg_send(struct xpdk_msg *msg);
int xpdk_msg_send_sync(struct xpdk_msg *msg);
int xpdk_msg_send_async(struct xpdk_msg *msg);
int xpdk_msg_process_poller(void *arg);

/* SPDK thread operations (called from SPDK thread only) */
void xpdk_spdk_handle_open(struct xpdk_msg *msg);
void xpdk_spdk_handle_close(struct xpdk_msg *msg);
void xpdk_spdk_handle_read(struct xpdk_msg *msg);
void xpdk_spdk_handle_write(struct xpdk_msg *msg);
void xpdk_spdk_handle_flush(struct xpdk_msg *msg);
void xpdk_spdk_handle_list_bdevs(struct xpdk_msg *msg);
void xpdk_spdk_handle_get_info(struct xpdk_msg *msg);
void xpdk_spdk_handle_qos_set_limits(struct xpdk_msg *msg);
void xpdk_spdk_handle_qos_get_limits(struct xpdk_msg *msg);
void xpdk_spdk_handle_get_perf_stats(struct xpdk_msg *msg);
void xpdk_spdk_handle_reset_perf_stats(struct xpdk_msg *msg);
void xpdk_spdk_handle_trim(struct xpdk_msg *msg);
void xpdk_spdk_handle_write_zeros(struct xpdk_msg *msg);

/* High-performance native vectored I/O operations */
void xpdk_spdk_handle_readv_native(struct xpdk_msg *msg);
void xpdk_spdk_handle_writev_native(struct xpdk_msg *msg);

/* Performance statistics functions */
int xpdk_perf_stats_init(struct xpdk_device *dev);
void xpdk_perf_stats_update(struct xpdk_device *dev, xpdk_io_type_t io_type, 
                           size_t bytes, uint64_t latency_us, bool success);

/* High-performance utility functions */
static inline uint64_t xpdk_get_time_us(void) {
    return spdk_get_ticks() / (spdk_get_ticks_hz() / 1000000);
}

/* Memory management for high-performance operations */
struct iovec *xpdk_alloc_iovec(int count);
void xpdk_free_iovec(struct iovec *iov);

/* Advanced open mechanism functions */
void xpdk_advanced_cleanup(void);

#endif /* XPDK_INTERNAL_H */