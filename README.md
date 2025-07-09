# libxpdk - SPDK Block Device Library

A C library that provides POSIX-like interfaces for SPDK (Storage Performance Development Kit) block devices, making it easier to work with high-performance storage devices.

## Features

- **POSIX-like API**: Familiar `open()`, `read()`, `write()`, `close()` style interfaces
- **Thread-safe**: All operations are thread-safe using proper locking
- **Synchronous and Asynchronous I/O**: Support for both blocking and non-blocking operations
- **Error handling**: Comprehensive error codes and messages
- **Device management**: Easy device discovery and information retrieval
- **Memory management**: Automatic buffer alignment and memory management

## API Overview

### Core Functions

```c
// Initialize XPDK options structure with defaults
void xpdk_opts_init(struct xpdk_opts *opts);

// Initialize the library (simple)
int xpdk_init(const char *config_file);

// Initialize the library with advanced options
int xpdk_init_opts(const struct xpdk_opts *opts);

// Cleanup and shutdown
void xpdk_cleanup(void);

// List available devices
int xpdk_list_bdevs(struct xpdk_bdev_info *devices, int max_devices);

// Open/close devices
xpdk_fd_t xpdk_open(const char *bdev_name, int flags);
int xpdk_close(xpdk_fd_t fd);

// Get device information
int xpdk_get_info(xpdk_fd_t fd, struct xpdk_bdev_info *info);
```

### Turbo Mode

libxpdk supports a **Turbo Mode** for maximum performance:

```c
struct xpdk_opts opts;
xpdk_opts_init(&opts);
opts.turbo_mode = true;        // Enable turbo mode
opts.cpu_core = 1;             // Bind to CPU core 1
opts.msg_ring_size = 2048;     // Larger message ring
opts.msg_pool_size = 2048;     // Larger message pool

int rc = xpdk_init_opts(&opts);
```

**Turbo Mode Features:**
- **Busy Polling**: Zero-latency message processing
- **CPU Binding**: Dedicated CPU core for consistent performance
- **Larger Buffers**: Increased ring and pool sizes
- **Batch Processing**: Process multiple messages per poll cycle

**When to Use Turbo Mode:**
- ✅ Latency-sensitive applications
- ✅ High-performance trading systems
- ✅ Real-time data processing
- ✅ Single-instance deployments with dedicated hardware

**When to Use Standard Mode:**
- ✅ Multi-instance deployments
- ✅ Resource-constrained environments
- ✅ Throughput-oriented applications
- ✅ General-purpose storage applications

### I/O Operations

```c
// Synchronous I/O
ssize_t xpdk_read(xpdk_fd_t fd, void *buffer, size_t count, uint64_t offset);
ssize_t xpdk_write(xpdk_fd_t fd, const void *buffer, size_t count, uint64_t offset);
int xpdk_flush(xpdk_fd_t fd);

// Asynchronous I/O
int xpdk_read_async(xpdk_fd_t fd, void *buffer, size_t count, uint64_t offset,
                    xpdk_io_callback_t callback, void *ctx);
int xpdk_write_async(xpdk_fd_t fd, const void *buffer, size_t count, uint64_t offset,
                     xpdk_io_callback_t callback, void *ctx);
int xpdk_poll(int max_completions);

// Vectored I/O (scatter-gather)
ssize_t xpdk_readv(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset);
ssize_t xpdk_writev(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset);
int xpdk_readv_async(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset,
                     xpdk_io_callback_t callback, void *ctx);
int xpdk_writev_async(xpdk_fd_t fd, const struct xpdk_iovec *iov, int iovcnt, uint64_t offset,
                      xpdk_io_callback_t callback, void *ctx);

// Advanced I/O operations
int xpdk_trim(xpdk_fd_t fd, uint64_t offset, uint64_t length);
int xpdk_write_zeros(xpdk_fd_t fd, uint64_t offset, uint64_t length);

// Batch I/O operations
struct xpdk_batch_ctx *xpdk_batch_init(int max_ios);
void xpdk_batch_cleanup(struct xpdk_batch_ctx *ctx);
int xpdk_batch_submit(struct xpdk_batch_ctx *ctx, struct xpdk_batch_io *ios, int count,
                      xpdk_io_callback_t callback);
int xpdk_batch_submit_one(struct xpdk_batch_ctx *ctx, const struct xpdk_batch_io *io);

// Buffer management
void *xpdk_alloc_buffer(size_t size);
void xpdk_free_buffer(void *buffer);

// Performance statistics
int xpdk_get_perf_stats(xpdk_fd_t fd, struct xpdk_perf_stats *stats);
int xpdk_reset_perf_stats(xpdk_fd_t fd);
```

### Error Handling

```c
// Get error string
const char *xpdk_strerror(int error_code);
```

### Quality of Service (QoS)

libxpdk supports SPDK native QoS controls for limiting I/O performance:

```c
// Set simple QoS limits
int xpdk_qos_enable_simple(xpdk_fd_t fd, uint64_t rw_iops_limit, uint64_t rw_bps_limit);

// Set advanced QoS limits
int xpdk_qos_set_rate_limits(xpdk_fd_t fd, const uint64_t *limits);

// Get current QoS limits
int xpdk_qos_get_rate_limits(xpdk_fd_t fd, uint64_t *limits);

// Disable QoS
int xpdk_qos_disable(xpdk_fd_t fd);
```

**QoS Rate Limit Types:**
- `XPDK_QOS_RW_IOPS_RATE_LIMIT`: IOPS limit for both read and write
- `XPDK_QOS_RW_BPS_RATE_LIMIT`: Bandwidth limit (bytes/sec) for both read and write
- `XPDK_QOS_R_BPS_RATE_LIMIT`: Bandwidth limit for reads only
- `XPDK_QOS_W_BPS_RATE_LIMIT`: Bandwidth limit for writes only

## Error Codes

- `XPDK_SUCCESS` (0): Success
- `XPDK_ERROR_INVALID` (-1): Invalid parameter
- `XPDK_ERROR_NOMEM` (-2): Out of memory
- `XPDK_ERROR_IO` (-3): I/O error
- `XPDK_ERROR_BUSY` (-4): Device busy
- `XPDK_ERROR_NODEV` (-5): No such device

## Data Structures

### I/O Vector for Vectored Operations

```c
struct xpdk_iovec {
    void *iov_base;  /* Buffer pointer */
    size_t iov_len;  /* Buffer length */
};
```

### Batch I/O Operation

```c
enum xpdk_io_type {
    XPDK_IO_READ,
    XPDK_IO_WRITE,
    XPDK_IO_FLUSH,
    XPDK_IO_TRIM,
    XPDK_IO_WRITE_ZEROS
};

struct xpdk_batch_io {
    enum xpdk_io_type type;           /* I/O operation type */
    xpdk_fd_t fd;                     /* File descriptor */
    void *buffer;                     /* Data buffer */
    size_t count;                     /* Number of bytes */
    uint64_t offset;                  /* Offset in device */
    xpdk_io_callback_t callback;     /* Completion callback */
    void *ctx;                        /* User context */
    int status;                       /* Operation status (output) */
};
```

### Performance Statistics

```c
struct xpdk_perf_stats {
    uint64_t total_ops;               /* Total operations completed */
    uint64_t total_bytes;             /* Total bytes transferred */
    uint64_t read_ops;                /* Read operations */
    uint64_t write_ops;               /* Write operations */
    uint64_t flush_ops;               /* Flush operations */
    uint64_t trim_ops;                /* TRIM operations */
    uint64_t write_zeros_ops;         /* Write zeros operations */
    uint64_t error_count;             /* Error count */
    double avg_latency_us;            /* Average latency in microseconds */
    double current_iops;              /* Current IOPS */
    double current_bps;               /* Current bandwidth (bytes/sec) */
    uint64_t collection_time_ms;      /* Statistics collection time */
};
```

### Initialization Options

```c
struct xpdk_opts {
    const char *config_file;          /* SPDK configuration file */
    bool turbo_mode;                  /* Enable turbo mode */
    int cpu_core;                     /* CPU core to bind to (-1 for no binding) */
    size_t msg_ring_size;             /* Message ring size */
    size_t msg_pool_size;             /* Message pool size */
    bool enable_stats;                /* Enable performance statistics */
    uint32_t stats_interval_ms;       /* Statistics collection interval */
};
```

## Advanced Features

### Turbo Mode
- **Purpose**: Maximum performance with minimal latency
- **Features**: Busy polling, CPU binding, larger buffers
- **Use Case**: Latency-sensitive applications

### Vectored I/O
- **Purpose**: Efficient scatter-gather operations
- **Benefits**: Reduce system calls, improve throughput
- **Use Case**: Large data transfers with multiple buffers

### Batch I/O
- **Purpose**: Submit multiple I/O operations as a single batch
- **Benefits**: Improved throughput, reduced overhead
- **Use Case**: High-throughput applications

### Performance Statistics
- **Purpose**: Real-time performance monitoring
- **Metrics**: IOPS, bandwidth, latency, operation counts
- **Use Case**: Performance tuning and monitoring

### Quality of Service (QoS)
- **Purpose**: Rate limiting and bandwidth control
- **Features**: IOPS limits, bandwidth limits, per-operation-type limits
- **Use Case**: Multi-tenant environments, resource management

## Building

### Prerequisites

- CMake 3.15 or later
- C11 compatible compiler (GCC, Clang)
- SPDK library and headers
- pkg-config

### Build Steps

```bash
mkdir build
cd build
cmake ..
make
```

### Build Options

- `BUILD_EXAMPLES=ON/OFF`: Build example programs (default: ON)
- `BUILD_TESTS=ON/OFF`: Build test programs (default: ON)

## Usage Examples

### Simple Read/Write

```c
#include "xpdk.h"

int main() {
    // Initialize library
    if (xpdk_init(NULL) != XPDK_SUCCESS) {
        return 1;
    }
    
    // Open device
    xpdk_fd_t fd = xpdk_open("Nvme0n1", O_RDWR);
    if (fd < 0) {
        xpdk_cleanup();
        return 1;
    }
    
    // Write data
    char data[4096] = "Hello, SPDK!";
    ssize_t written = xpdk_write(fd, data, sizeof(data), 0);
    
    // Read data back
    char read_buf[4096];
    ssize_t read_bytes = xpdk_read(fd, read_buf, sizeof(read_buf), 0);
    
    // Cleanup
    xpdk_close(fd);
    xpdk_cleanup();
    return 0;
}
```

### Turbo Mode Example

```c
#include "xpdk.h"

int main() {
    struct xpdk_opts opts;
    
    // Configure turbo mode
    xpdk_opts_init(&opts);
    opts.turbo_mode = true;        // Enable turbo mode
    opts.cpu_core = 1;             // Bind to CPU core 1
    opts.msg_ring_size = 2048;     // Larger buffers
    opts.msg_pool_size = 2048;
    
    // Initialize with turbo mode
    if (xpdk_init_opts(&opts) != XPDK_SUCCESS) {
        return 1;
    }
    
    // ... rest of the code is the same ...
    
    xpdk_cleanup();
    return 0;
}
```

### Asynchronous I/O

```c
#include "xpdk.h"

void async_callback(void *ctx, int status) {
    printf("I/O completed with status: %s\n", xpdk_strerror(status));
    *(int*)ctx = 1;  // Signal completion
}

int main() {
    xpdk_init(NULL);
    xpdk_fd_t fd = xpdk_open("Nvme0n1", O_RDWR);
    
    char data[4096] = "Async Hello!";
    volatile int completed = 0;
    
    // Start async write
    xpdk_write_async(fd, data, sizeof(data), 0, async_callback, (void*)&completed);
    
    // Poll for completion
    while (!completed) {
        xpdk_poll(10);
        usleep(1000);
    }
    
    xpdk_close(fd);
    xpdk_cleanup();
    return 0;
}
```

### QoS (Quality of Service) Example

```c
#include "xpdk.h"

int main() {
    xpdk_init(NULL);
    xpdk_fd_t fd = xpdk_open("Nvme0n1", O_RDWR);
    
    // Enable simple QoS: 1000 IOPS, 4MB/s bandwidth
    xpdk_qos_enable_simple(fd, 1000, 4 * 1024 * 1024);
    
    // Perform I/O operations (will be rate-limited)
    char data[4096] = "QoS test data";
    xpdk_write(fd, data, sizeof(data), 0);
    
    // Set advanced QoS limits
    uint64_t limits[XPDK_QOS_NUM_RATE_LIMIT_TYPES] = {0};
    limits[XPDK_QOS_RW_IOPS_RATE_LIMIT] = 500;           // 500 IOPS
    limits[XPDK_QOS_R_BPS_RATE_LIMIT] = 2 * 1024 * 1024; // 2MB/s reads
    limits[XPDK_QOS_W_BPS_RATE_LIMIT] = 1 * 1024 * 1024; // 1MB/s writes
    
    xpdk_qos_set_rate_limits(fd, limits);
    
    // Get current limits
    uint64_t current_limits[XPDK_QOS_NUM_RATE_LIMIT_TYPES];
    xpdk_qos_get_rate_limits(fd, current_limits);
    
    // Disable QoS
    xpdk_qos_disable(fd);
    
    xpdk_close(fd);
    xpdk_cleanup();
    return 0;
}
```

### Vectored I/O (Scatter-Gather) Example

```c
#include "xpdk.h"

int main() {
    xpdk_init(NULL);
    xpdk_fd_t fd = xpdk_open("Nvme0n1", O_RDWR);
    
    /* Prepare multiple buffers for vectored I/O */
    char *buffer1 = xpdk_alloc_buffer(4096);
    char *buffer2 = xpdk_alloc_buffer(4096);
    char *buffer3 = xpdk_alloc_buffer(4096);
    
    strcpy(buffer1, "First chunk of data");
    strcpy(buffer2, "Second chunk of data");
    strcpy(buffer3, "Third chunk of data");
    
    /* Setup I/O vector */
    struct xpdk_iovec iov[3] = {
        { .iov_base = buffer1, .iov_len = 4096 },
        { .iov_base = buffer2, .iov_len = 4096 },
        { .iov_base = buffer3, .iov_len = 4096 }
    };
    
    /* Write multiple buffers in a single operation */
    ssize_t written = xpdk_writev(fd, iov, 3, 0);
    printf("Vectored write: %zd bytes\n", written);
    
    /* Read back using vectored I/O */
    char *read_buf1 = xpdk_alloc_buffer(4096);
    char *read_buf2 = xpdk_alloc_buffer(4096);
    char *read_buf3 = xpdk_alloc_buffer(4096);
    
    struct xpdk_iovec read_iov[3] = {
        { .iov_base = read_buf1, .iov_len = 4096 },
        { .iov_base = read_buf2, .iov_len = 4096 },
        { .iov_base = read_buf3, .iov_len = 4096 }
    };
    
    ssize_t read_bytes = xpdk_readv(fd, read_iov, 3, 0);
    printf("Vectored read: %zd bytes\n", read_bytes);
    
    /* Cleanup */
    xpdk_free_buffer(buffer1);
    xpdk_free_buffer(buffer2);
    xpdk_free_buffer(buffer3);
    xpdk_free_buffer(read_buf1);
    xpdk_free_buffer(read_buf2);
    xpdk_free_buffer(read_buf3);
    
    xpdk_close(fd);
    xpdk_cleanup();
    return 0;
}
```

### Batch I/O Example

```c
#include "xpdk.h"

static volatile int completed_ops = 0;

void batch_callback(void *ctx, int status) {
    int *op_id = (int *)ctx;
    printf("Operation %d completed: %s\n", *op_id, xpdk_strerror(status));
    __sync_fetch_and_add(&completed_ops, 1);
}

int main() {
    xpdk_init(NULL);
    xpdk_fd_t fd = xpdk_open("Nvme0n1", O_RDWR);
    
    /* Initialize batch context */
    struct xpdk_batch_ctx *batch_ctx = xpdk_batch_init(16);
    
    /* Prepare batch operations */
    const int BATCH_SIZE = 8;
    struct xpdk_batch_io ios[BATCH_SIZE];
    char *buffers[BATCH_SIZE];
    int op_ids[BATCH_SIZE];
    
    for (int i = 0; i < BATCH_SIZE; i++) {
        buffers[i] = xpdk_alloc_buffer(4096);
        snprintf(buffers[i], 4096, "Batch operation %d data", i);
        op_ids[i] = i;
        
        ios[i].type = XPDK_IO_WRITE;
        ios[i].fd = fd;
        ios[i].buffer = buffers[i];
        ios[i].count = 4096;
        ios[i].offset = i * 4096;
        ios[i].callback = batch_callback;
        ios[i].ctx = &op_ids[i];
    }
    
    /* Submit the entire batch */
    int rc = xpdk_batch_submit(batch_ctx, ios, BATCH_SIZE, NULL);
    if (rc == XPDK_SUCCESS) {
        /* Wait for all operations to complete */
        while (completed_ops < BATCH_SIZE) {
            xpdk_poll(10);
            usleep(1000);
        }
        printf("All %d batch operations completed\n", BATCH_SIZE);
    }
    
    /* Alternative: Submit operations one by one */
    for (int i = 0; i < BATCH_SIZE; i++) {
        ios[i].offset = (i + BATCH_SIZE) * 4096;  /* Different offset */
        xpdk_batch_submit_one(batch_ctx, &ios[i]);
    }
    
    /* Cleanup */
    xpdk_batch_cleanup(batch_ctx);
    for (int i = 0; i < BATCH_SIZE; i++) {
        xpdk_free_buffer(buffers[i]);
    }
    
    xpdk_close(fd);
    xpdk_cleanup();
    return 0;
}
```

### Performance Statistics Example

```c
#include "xpdk.h"

int main() {
    xpdk_init(NULL);
    xpdk_fd_t fd = xpdk_open("Nvme0n1", O_RDWR);
    
    /* Reset performance counters */
    xpdk_reset_perf_stats(fd);
    
    /* Perform some I/O operations */
    char *buffer = xpdk_alloc_buffer(4096);
    strcpy(buffer, "Performance test data");
    
    for (int i = 0; i < 1000; i++) {
        xpdk_write(fd, buffer, 4096, i * 4096);
    }
    
    /* Get performance statistics */
    struct xpdk_perf_stats stats;
    int rc = xpdk_get_perf_stats(fd, &stats);
    if (rc == XPDK_SUCCESS) {
        printf("Performance Statistics:\n");
        printf("  Total operations: %lu\n", stats.total_ops);
        printf("  Total bytes: %lu MB\n", stats.total_bytes / (1024 * 1024));
        printf("  Read operations: %lu\n", stats.read_ops);
        printf("  Write operations: %lu\n", stats.write_ops);
        printf("  Average latency: %.2f μs\n", stats.avg_latency_us);
        printf("  Current IOPS: %.2f\n", stats.current_iops);
        printf("  Current bandwidth: %.2f MB/s\n", stats.current_bps / (1024.0 * 1024.0));
        printf("  Error count: %lu\n", stats.error_count);
    }
    
    xpdk_free_buffer(buffer);
    xpdk_close(fd);
    xpdk_cleanup();
    return 0;
}
```

### TRIM and Write Zeroes Example

```c
#include "xpdk.h"

int main() {
    xpdk_init(NULL);
    xpdk_fd_t fd = xpdk_open("Nvme0n1", O_RDWR);
    
    /* Write some data first */
    char *buffer = xpdk_alloc_buffer(64 * 1024);  /* 64KB */
    memset(buffer, 0xAA, 64 * 1024);
    xpdk_write(fd, buffer, 64 * 1024, 0);
    
    /* TRIM operation - deallocate blocks */
    printf("Performing TRIM operation...\n");
    int rc = xpdk_trim(fd, 0, 32 * 1024);  /* TRIM first 32KB */
    if (rc == XPDK_SUCCESS) {
        printf("TRIM operation completed successfully\n");
    } else {
        printf("TRIM failed: %s\n", xpdk_strerror(rc));
    }
    
    /* Write zeros operation */
    printf("Performing write zeros operation...\n");
    rc = xpdk_write_zeros(fd, 32 * 1024, 32 * 1024);  /* Zero next 32KB */
    if (rc == XPDK_SUCCESS) {
        printf("Write zeros operation completed successfully\n");
    } else {
        printf("Write zeros failed: %s\n", xpdk_strerror(rc));
    }
    
    xpdk_free_buffer(buffer);
    xpdk_close(fd);
    xpdk_cleanup();
    return 0;
}
```

### Device Discovery

```c
#include "xpdk.h"

int main() {
    xpdk_init(NULL);
    
    struct xpdk_bdev_info devices[32];
    int count = xpdk_list_bdevs(devices, 32);
    
    printf("Found %d devices:\n", count);
    for (int i = 0; i < count; i++) {
        printf("  %s: %lu blocks, %lu bytes each\n",
               devices[i].name, devices[i].num_blocks, devices[i].block_size);
    }
    
    xpdk_cleanup();
    return 0;
}
```

## Examples

The `examples/` directory contains complete example programs:

- `simple_example`: Basic synchronous I/O operations
- `async_example`: Asynchronous I/O operations
- `list_devices`: Device discovery and information
- `turbo_example`: Demonstrates turbo mode usage
- `turbo_benchmark`: Performance comparison between standard and turbo modes
- `qos_example`: QoS (Quality of Service) configuration and usage
- `vectored_example`: Vectored I/O (scatter-gather) operations
- `batch_example`: Batch I/O operations and performance statistics
- `advanced_benchmark`: Comprehensive performance benchmark of all features

### Running Examples

```bash
# List available devices
./examples/list_devices

# Simple I/O example
./examples/simple_example Nvme0n1

# Turbo mode example
./examples/turbo_example Nvme0n1 --turbo --cpu-core 1

# Performance benchmark
./examples/turbo_benchmark Nvme0n1 both

# QoS example
./examples/qos_example

# Vectored I/O example
./examples/vectored_example Nvme0n1

# Batch I/O and performance statistics example
./examples/batch_example Nvme0n1

# Advanced performance benchmark (all features)
./examples/advanced_benchmark Nvme0n1
./examples/advanced_benchmark Nvme0n1 --turbo --cpu-core 2
```

## Testing

Run the test suite:

```bash
make test
```

Or run individual tests:

```bash
./tests/test_basic
./tests/test_io
./tests/test_async
./tests/test_batch
./tests/test_qos
./tests/test_integration
```

## SPDK Configuration

libxpdk requires SPDK to be properly configured. You can either:

1. Use the default configuration by passing `NULL` to `xpdk_init()`
2. Provide a custom SPDK configuration file path

Example SPDK configuration file:

```json
{
  "subsystems": [
    {
      "subsystem": "bdev",
      "config": [
        {
          "method": "bdev_nvme_attach_controller",
          "params": {
            "trtype": "PCIe",
            "name": "Nvme0",
            "traddr": "0000:01:00.0"
          }
        }
      ]
    }
  ]
}
```

## Thread Safety

All libxpdk functions are thread-safe. The library uses internal locking to ensure safe concurrent access to devices and I/O operations.

## Memory Management

The library automatically handles:
- Buffer alignment for SPDK requirements
- Memory allocation/deallocation for I/O buffers
- Proper cleanup of resources

## Contributing

1. Follow the existing code style (snake_case, proper error handling)
2. Add tests for new functionality
3. Update documentation
4. Ensure all tests pass

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Design Principles

- **Simplicity**: Hide SPDK complexity behind familiar interfaces
- **Performance**: Minimal overhead over raw SPDK operations
- **Reliability**: Comprehensive error handling and resource management
- **Compatibility**: POSIX-like semantics for easy adoption
