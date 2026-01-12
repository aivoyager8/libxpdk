#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/uio.h>
#include <spdk/queue.h>

/* 复制相关的结构体定义 */
enum xpdk_msg_type {
    XPDK_MSG_OPEN,
    XPDK_MSG_CLOSE,
    XPDK_MSG_READ,
    XPDK_MSG_WRITE,
    XPDK_MSG_READV_NATIVE,
    XPDK_MSG_WRITEV_NATIVE,
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

typedef void (*xpdk_io_callback_t)(void *ctx, int status, size_t bytes_transferred);

struct xpdk_bdev_info {
    char name[256];
    uint64_t size_bytes;
    uint32_t block_size;
    bool supports_trim;
    bool supports_write_zeros;
};

struct xpdk_perf_stats {
    uint64_t read_ops;
    uint64_t write_ops;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t read_latency_us;
    uint64_t write_latency_us;
    uint64_t error_count;
    double iops;
    double throughput_mbps;
};

struct xpdk_msg {
    enum xpdk_msg_type type;
    void *ctx;
    
    volatile bool completed;
    int status;
    
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
            void *buffer;
            size_t count;
            uint64_t offset;
            ssize_t bytes_transferred;
            xpdk_io_callback_t callback;
            void *user_ctx;
            void *spdk_ctx;
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
            uint64_t *limits;
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

int main()
{
    printf("xpdk_msg结构体大小分析:\n");
    printf("sizeof(struct xpdk_msg) = %zu bytes\n", sizeof(struct xpdk_msg));
    printf("sizeof(enum xpdk_msg_type) = %zu bytes\n", sizeof(enum xpdk_msg_type));
    printf("sizeof(void*) = %zu bytes\n", sizeof(void*));
    printf("sizeof(bool) = %zu bytes\n", sizeof(bool));
    printf("sizeof(int) = %zu bytes\n", sizeof(int));
    printf("sizeof(TAILQ_ENTRY) = %zu bytes\n", sizeof(TAILQ_ENTRY(xpdk_msg)));
    
    printf("\n联合体内各成员大小:\n");
    struct xpdk_msg msg;
    printf("sizeof(msg.open) = %zu bytes\n", sizeof(msg.open));
    printf("sizeof(msg.close) = %zu bytes\n", sizeof(msg.close));
    printf("sizeof(msg.io) = %zu bytes\n", sizeof(msg.io));
    printf("sizeof(msg.list) = %zu bytes\n", sizeof(msg.list));
    printf("sizeof(msg.get_info) = %zu bytes\n", sizeof(msg.get_info));
    printf("sizeof(msg.qos_limits) = %zu bytes\n", sizeof(msg.qos_limits));
    printf("sizeof(msg.perf_stats) = %zu bytes\n", sizeof(msg.perf_stats));
    printf("sizeof(msg.trim) = %zu bytes\n", sizeof(msg.trim));
    
    printf("\n内存对齐分析:\n");
    printf("offsetof(type) = %zu\n", offsetof(struct xpdk_msg, type));
    printf("offsetof(ctx) = %zu\n", offsetof(struct xpdk_msg, ctx));
    printf("offsetof(completed) = %zu\n", offsetof(struct xpdk_msg, completed));
    printf("offsetof(status) = %zu\n", offsetof(struct xpdk_msg, status));
    printf("offsetof(union) = %zu\n", offsetof(struct xpdk_msg, open));
    printf("offsetof(link) = %zu\n", offsetof(struct xpdk_msg, link));
    
    return 0;
}
