#define _GNU_SOURCE#define XPDK_DEFAULT_RING_SIZE      512
#define XPDK_DEFAULT_POLL_PERIOD_US 1000include "xpdk_internal.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <spdk/env.h>
#include <spdk/event.h>
#include <spdk/thread.h>
// Note: spdk/ring.h is not available in this SPDK version
// We'll implement ring functionality using other SPDK APIs

/* Global context */
struct xpdk_context g_xpdk_ctx = {0};

/* Default configuration values */
#define XPDK_DEFAULT_RING_SIZE      1024
#define XPDK_DEFAULT_POOL_SIZE      1024
#define XPDK_DEFAULT_POLL_PERIOD_US 1000
#define XPDK_TURBO_POLL_PERIOD_US   0      /* Busy polling */

/* Error strings */
static const char *error_strings[] = {
    "Success",
    "Invalid parameter", 
    "Out of memory",
    "I/O error",
    "Device busy",
    "No such device",
    "QoS error"
};

/* Set CPU affinity for current thread */
int
xpdk_set_cpu_affinity(int cpu_core)
{
    if (cpu_core < 0) {
        return XPDK_SUCCESS; /* No binding requested */
    }
    
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_core, &cpuset);
    
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) != 0) {
        printf("Warning: Failed to bind to CPU core %d\n", cpu_core);
        return XPDK_ERROR_IO;
    }
    
    printf("SPDK thread bound to CPU core %d\n", cpu_core);
#else
    printf("Warning: CPU affinity not supported on this platform\n");
#endif
    
    return XPDK_SUCCESS;
}

/* Print performance statistics */
void
xpdk_print_performance_stats(void)
{
    printf("XPDK Performance Statistics:\n");
    printf("  Mode: %s\n", g_xpdk_ctx.opts.turbo_mode ? "Turbo (High Performance)" : "Standard");
    printf("  CPU Binding: %s\n", g_xpdk_ctx.opts.cpu_core >= 0 ? "Enabled" : "Disabled");
    printf("  Total Messages: %lu\n", g_xpdk_ctx.total_messages_processed);
    printf("  Total I/O Ops: %lu\n", g_xpdk_ctx.total_io_operations);
    printf("  Ring Size: %u\n", g_xpdk_ctx.opts.msg_ring_size);
    printf("  Pool Size: %u\n", g_xpdk_ctx.opts.msg_pool_size);
    printf("  Poll Period: %u us\n", g_xpdk_ctx.opts.poll_period_us);
}

/* Message processing poller - runs in SPDK thread */
int
xpdk_msg_process_poller(void *arg)
{
    (void)arg; /* unused parameter */
    
    struct xpdk_msg *msg;
    size_t count;
    int processed = 0;
    
    /* In turbo mode, process multiple messages per poll */
    int max_batch = g_xpdk_ctx.opts.turbo_mode ? 32 : 1;
    
    for (int i = 0; i < max_batch; i++) {
        /* Dequeue messages from ring */
        count = spdk_ring_dequeue(g_xpdk_ctx.msg_ring, (void **)&msg, 1);
        if (count == 0) {
            break; /* No more messages */
        }
        
        processed++;
        g_xpdk_ctx.total_messages_processed++;
        
        /* Process message based on type - optimized for high performance */
        switch (msg->type) {
        case XPDK_MSG_OPEN:
            xpdk_spdk_handle_open(msg);
            break;
        case XPDK_MSG_CLOSE:
            xpdk_spdk_handle_close(msg);
            break;
        case XPDK_MSG_READ:
        case XPDK_MSG_WRITE:
            g_xpdk_ctx.total_io_operations++;
            if (msg->type == XPDK_MSG_READ) {
                xpdk_spdk_handle_read(msg);
            } else {
                xpdk_spdk_handle_write(msg);
            }
            break;
        case XPDK_MSG_READV_NATIVE:
        case XPDK_MSG_WRITEV_NATIVE:
            g_xpdk_ctx.total_io_operations++;
            if (msg->type == XPDK_MSG_READV_NATIVE) {
                xpdk_spdk_handle_readv_native(msg);
            } else {
                xpdk_spdk_handle_writev_native(msg);
            }
            break;
        case XPDK_MSG_FLUSH:
            xpdk_spdk_handle_flush(msg);
            break;
        case XPDK_MSG_LIST_BDEVS:
            xpdk_spdk_handle_list_bdevs(msg);
            break;
        case XPDK_MSG_GET_INFO:
            xpdk_spdk_handle_get_info(msg);
            break;
        case XPDK_MSG_QOS_SET_LIMITS:
            xpdk_spdk_handle_qos_set_limits(msg);
            break;
        case XPDK_MSG_QOS_GET_LIMITS:
            xpdk_spdk_handle_qos_get_limits(msg);
            break;
        case XPDK_MSG_GET_PERF_STATS:
            xpdk_spdk_handle_get_perf_stats(msg);
            break;
        case XPDK_MSG_RESET_PERF_STATS:
            xpdk_spdk_handle_reset_perf_stats(msg);
            break;
        case XPDK_MSG_TRIM:
            xpdk_spdk_handle_trim(msg);
            break;
        case XPDK_MSG_WRITE_ZEROS:
            xpdk_spdk_handle_write_zeros(msg);
            break;
        case XPDK_MSG_SHUTDOWN:
            g_xpdk_ctx.spdk_thread_running = false;
            msg->completed = true;
            return SPDK_POLLER_BUSY;
        default:
            msg->status = XPDK_ERROR_INVALID;
            msg->completed = true;
            break;
        }
    }
    
    return processed > 0 ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}

/* SPDK thread main function */
void *
xpdk_spdk_thread_main(void *arg)
{
    (void)arg; /* unused parameter */
    
    struct spdk_thread *thread;
    uint64_t poll_period_us;
    int rc;
    
    /* Set CPU affinity if requested */
    xpdk_set_cpu_affinity(g_xpdk_ctx.opts.cpu_core);
    
    /* Initialize SPDK thread library in this thread */
    rc = spdk_thread_lib_init_ext(NULL, NULL, 0, 64);  /* Use small pool size */
    if (rc < 0) {
        printf("Failed to initialize SPDK thread library in worker thread\n");
        return NULL;
    }
    
    /* Allocate SPDK thread */
    thread = spdk_thread_create("xpdk_main", NULL);
    if (thread == NULL) {
        printf("Failed to create SPDK thread\n");
        spdk_thread_lib_fini();
        return NULL;
    }
    
    g_xpdk_ctx.spdk_thread = thread;
    
    /* Determine polling period based on turbo mode */
    poll_period_us = g_xpdk_ctx.opts.turbo_mode ? XPDK_TURBO_POLL_PERIOD_US : g_xpdk_ctx.opts.poll_period_us;
    
    /* Register message processing poller */
    g_xpdk_ctx.msg_poller = spdk_poller_register(xpdk_msg_process_poller, NULL, poll_period_us);
    if (g_xpdk_ctx.msg_poller == NULL) {
        printf("Failed to register message poller\n");
        spdk_thread_exit(thread);
        return NULL;
    }
    
    printf("SPDK thread started in %s mode (poll period: %lu us)\n", 
           g_xpdk_ctx.opts.turbo_mode ? "TURBO" : "STANDARD", poll_period_us);
    g_xpdk_ctx.spdk_thread_running = true;
    
    /* Main event loop */
    while (g_xpdk_ctx.spdk_thread_running) {
        spdk_thread_poll(thread, 0, 0);
        
        /* In standard mode, add small sleep to reduce CPU usage */
        if (!g_xpdk_ctx.opts.turbo_mode && g_xpdk_ctx.opts.poll_period_us > 0) {
            usleep(g_xpdk_ctx.opts.poll_period_us);
        }
    }
    
    /* Cleanup */
    spdk_poller_unregister(&g_xpdk_ctx.msg_poller);
    spdk_thread_exit(thread);
    spdk_thread_lib_fini();  /* Cleanup thread library in same thread */
    
    printf("SPDK thread exited\n");
    return NULL;
}

/* Initialize SPDK thread */
int
xpdk_init_spdk_thread(const struct xpdk_opts *opts)
{
    struct spdk_env_opts env_opts;
    int rc;
    
    /* Initialize SPDK environment */
    spdk_env_opts_init(&env_opts);
    env_opts.name = "xpdk";
    env_opts.shm_id = -1;
    env_opts.mem_size = 512;  /* Allocate 512MB for SPDK */
    
    rc = spdk_env_init(&env_opts);
    if (rc < 0) {
        printf("Failed to initialize SPDK environment\n");
        return XPDK_ERROR_IO;
    }
    
    /* Create message ring (lock-free queue) */
    uint32_t ring_size = opts->msg_ring_size > 0 ? opts->msg_ring_size : XPDK_DEFAULT_RING_SIZE;
    g_xpdk_ctx.msg_ring = spdk_ring_create(SPDK_RING_TYPE_MP_SC, ring_size, SPDK_ENV_SOCKET_ID_ANY);
    if (g_xpdk_ctx.msg_ring == NULL) {
        printf("Failed to create message ring (size: %u)\n", ring_size);
        return XPDK_ERROR_NOMEM;
    }
    
    printf("Created message ring (size: %u)\n", ring_size);
    
    /* Start SPDK thread */
    rc = pthread_create(&g_xpdk_ctx.spdk_thread_id, NULL, 
                        xpdk_spdk_thread_main, NULL);
    if (rc != 0) {
        printf("Failed to create SPDK thread\n");
        spdk_ring_free(g_xpdk_ctx.msg_ring);
        return XPDK_ERROR_IO;
    }
    
    /* Wait for SPDK thread to be ready */
    while (!g_xpdk_ctx.spdk_thread_running) {
        usleep(1000);
    }
    
    return XPDK_SUCCESS;
}

/* Cleanup SPDK thread */
void
xpdk_cleanup_spdk_thread(void)
{
    if (!g_xpdk_ctx.spdk_thread_running) {
        return;
    }
    
    /* Send shutdown message */
    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_SHUTDOWN);
    if (msg != NULL) {
        xpdk_msg_send_sync(msg);
        xpdk_msg_free(msg);
    }
    
    /* Wait for SPDK thread to exit */
    pthread_join(g_xpdk_ctx.spdk_thread_id, NULL);
    
    /* Note: spdk_thread_lib_fini() is called in the worker thread */
    
    /* Cleanup resources */
    if (g_xpdk_ctx.msg_ring != NULL) {
        spdk_ring_free(g_xpdk_ctx.msg_ring);
        g_xpdk_ctx.msg_ring = NULL;
    }
}

void
xpdk_opts_init(struct xpdk_opts *opts)
{
    if (opts == NULL) {
        return;
    }
    
    memset(opts, 0, sizeof(*opts));
    opts->config_file = NULL;
    opts->turbo_mode = false;          /* Default: standard mode */
    opts->cpu_core = -1;               /* Default: no CPU binding */
    opts->msg_ring_size = 0;           /* Default: use XPDK_DEFAULT_RING_SIZE */
    opts->poll_period_us = XPDK_DEFAULT_POLL_PERIOD_US; /* Default: 1ms polling */
}

int
xpdk_init(const char *config_file)
{
    struct xpdk_opts opts;
    
    /* Initialize with default options */
    xpdk_opts_init(&opts);
    opts.config_file = config_file;
    
    return xpdk_init_opts(&opts);
}

int
xpdk_init_opts(const struct xpdk_opts *opts)
{
    int rc;

    /* Check if already initialized */
    if (g_xpdk_ctx.initialized) {
        return XPDK_SUCCESS;
    }

    if (opts == NULL) {
        return XPDK_ERROR_INVALID;
    }

    /* Initialize global context */
    memset(&g_xpdk_ctx, 0, sizeof(g_xpdk_ctx));
    
    /* Copy options */
    memcpy(&g_xpdk_ctx.opts, opts, sizeof(*opts));
    
    /* Set defaults for zero values */
    if (g_xpdk_ctx.opts.msg_ring_size == 0) {
        g_xpdk_ctx.opts.msg_ring_size = XPDK_DEFAULT_RING_SIZE;
    }
    if (g_xpdk_ctx.opts.poll_period_us == 0 && !g_xpdk_ctx.opts.turbo_mode) {
        g_xpdk_ctx.opts.poll_period_us = XPDK_DEFAULT_POLL_PERIOD_US;
    }
    
    printf("Initializing XPDK library:\n");
    printf("  Mode: %s\n", g_xpdk_ctx.opts.turbo_mode ? "TURBO" : "STANDARD");
    printf("  CPU Core: %s\n", g_xpdk_ctx.opts.cpu_core >= 0 ? "Bound" : "Unbound");
    printf("  Config: %s\n", g_xpdk_ctx.opts.config_file ? g_xpdk_ctx.opts.config_file : "Default");
    
    /* Initialize synchronization */
    rc = pthread_mutex_init(&g_xpdk_ctx.sync_lock, NULL);
    if (rc != 0) {
        return XPDK_ERROR_NOMEM;
    }
    
    rc = pthread_cond_init(&g_xpdk_ctx.sync_cond, NULL);
    if (rc != 0) {
        pthread_mutex_destroy(&g_xpdk_ctx.sync_lock);
        return XPDK_ERROR_NOMEM;
    }

    /* Initialize device slots */
    for (int i = 0; i < XPDK_MAX_OPEN_DEVICES; i++) {
        g_xpdk_ctx.devices[i].fd = i;
        g_xpdk_ctx.devices[i].in_use = false;
    }

    /* Initialize SPDK thread */
    rc = xpdk_init_spdk_thread(&g_xpdk_ctx.opts);
    if (rc != XPDK_SUCCESS) {
        pthread_cond_destroy(&g_xpdk_ctx.sync_cond);
        pthread_mutex_destroy(&g_xpdk_ctx.sync_lock);
        return rc;
    }

    g_xpdk_ctx.initialized = true;
    printf("XPDK library initialized successfully\n");
    
    return XPDK_SUCCESS;
}

void
xpdk_cleanup(void)
{
    if (!g_xpdk_ctx.initialized) {
        return;
    }

    printf("Shutting down XPDK library...\n");
    
    /* Print performance statistics */
    if (g_xpdk_ctx.total_messages_processed > 0) {
        xpdk_print_performance_stats();
    }

    /* Close all open devices */
    for (int i = 0; i < XPDK_MAX_OPEN_DEVICES; i++) {
        if (g_xpdk_ctx.devices[i].in_use) {
            xpdk_close(i);
        }
    }

    /* Cleanup SPDK thread */
    xpdk_cleanup_spdk_thread();

    /* Cleanup advanced open mechanism */
    xpdk_advanced_cleanup();

    /* Cleanup synchronization */
    pthread_cond_destroy(&g_xpdk_ctx.sync_cond);
    pthread_mutex_destroy(&g_xpdk_ctx.sync_lock);

    g_xpdk_ctx.initialized = false;
    printf("XPDK library shutdown complete\n");
}

/* Message allocation */
struct xpdk_msg *
xpdk_msg_alloc(enum xpdk_msg_type type)
{
    struct xpdk_msg *msg;
    
    /* Use standard malloc for message allocation */
    msg = malloc(sizeof(struct xpdk_msg));
    if (msg == NULL) {
        return NULL;
    }
    
    memset(msg, 0, sizeof(*msg));
    msg->type = type;
    msg->completed = false;
    msg->status = XPDK_SUCCESS;
    
    return msg;
}

/* Message deallocation */
void
xpdk_msg_free(struct xpdk_msg *msg)
{
    if (msg != NULL) {
        free(msg);
    }
}

/* Send message synchronously */
int
xpdk_msg_send_sync(struct xpdk_msg *msg)
{
    size_t count;
    
    if (msg == NULL) {
        return XPDK_ERROR_INVALID;
    }
    
    /* Enqueue message to ring */
    count = spdk_ring_enqueue(g_xpdk_ctx.msg_ring, (void **)&msg, 1, NULL);
    if (count != 1) {
        return XPDK_ERROR_BUSY;
    }
    
    /* Wait for completion using busy wait for maximum performance */
    while (!msg->completed) {
        sched_yield(); /* Yield CPU but stay responsive */
    }
    
    return msg->status;
}

/* Send message asynchronously */
int
xpdk_msg_send_async(struct xpdk_msg *msg)
{
    size_t count;
    
    if (msg == NULL) {
        return XPDK_ERROR_INVALID;
    }
    
    /* Enqueue message to ring */
    count = spdk_ring_enqueue(g_xpdk_ctx.msg_ring, (void **)&msg, 1, NULL);
    if (count != 1) {
        return XPDK_ERROR_BUSY;
    }
    
    return XPDK_SUCCESS;
}

int
xpdk_find_free_fd(void)
{
    for (int i = 0; i < XPDK_MAX_OPEN_DEVICES; i++) {
        if (!g_xpdk_ctx.devices[i].in_use) {
            g_xpdk_ctx.devices[i].in_use = true;
            return i;
        }
    }
    
    return XPDK_ERROR_NOMEM;
}

struct xpdk_device *
xpdk_get_device(xpdk_fd_t fd)
{
    if (fd < 0 || fd >= XPDK_MAX_OPEN_DEVICES) {
        return NULL;
    }

    struct xpdk_device *dev = &g_xpdk_ctx.devices[fd];
    if (!dev->in_use) {
        return NULL;
    }

    return dev;
}

void
xpdk_put_device(struct xpdk_device *dev)
{
    if (dev == NULL) {
        return;
    }

    dev->in_use = false;
}

const char *
xpdk_strerror(int error_code)
{
    int index = -error_code;
    if (index < 0 || index >= (int)(sizeof(error_strings) / sizeof(error_strings[0]))) {
        return "Unknown error";
    }
    return error_strings[index];
}
