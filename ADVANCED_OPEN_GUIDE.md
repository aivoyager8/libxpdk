# Advanced Open Mechanism - User Guide

## Overview

The libxpdk Advanced Open Mechanism provides a comprehensive, high-performance solution for complex disk/bdev initialization scenarios. This feature extends the basic `xpdk_open()` functionality with advanced capabilities including batch operations, dependency resolution, custom device creation, and sophisticated error handling.

## Key Features

### 🚀 **Batch Device Operations**
- Open multiple devices in a single operation
- Automatic dependency resolution
- Atomic operations (all succeed or all fail)
- Optimized for high-performance scenarios

### 🔗 **Device Dependencies**
- Define complex device relationships
- Automatic dependency ordering
- Validation of dependency chains
- Support for hierarchical device structures

### ⚙️ **Custom Device Creation**
- Plugin-based device creation system
- Support for RAID, LVM, and custom configurations
- Runtime device configuration
- Extensible architecture

### 🛡️ **Advanced Error Handling**
- Detailed error reporting with context
- Automatic retry mechanisms
- Graceful degradation strategies
- Comprehensive rollback operations

### 📊 **Performance Optimization**
- Parallel device initialization
- NUMA-aware resource allocation
- Zero-copy operations where possible
- CPU affinity management

## API Reference

### Core Types

```c
/* Advanced open options */
struct xpdk_open_opts {
    int flags;                          /* Open flags (O_RDONLY, O_WRONLY, O_RDWR) */
    uint32_t timeout_ms;                /* Operation timeout */
    bool allow_partial_failure;        /* Allow partial success in batch operations */
    bool enable_turbo;                  /* Enable turbo mode */
    int cpu_core;                       /* CPU core affinity (-1 for default) */
    uint32_t queue_depth;               /* I/O queue depth */
    struct xpdk_qos_limits *qos_limits; /* QoS limits */
    bool enable_auto_retry;             /* Enable automatic retry */
    uint32_t max_retry_count;           /* Maximum retry attempts */
    uint32_t retry_delay_ms;            /* Delay between retries */
};

/* Device descriptor with extended information */
struct xpdk_device_desc {
    xpdk_fd_t fd;                       /* File descriptor */
    char name[256];                     /* Device name */
    struct xpdk_bdev_info info;         /* Device information */
    xpdk_device_state_t state;          /* Device state */
    uint32_t group_id;                  /* Group identifier */
    uint32_t dependency_count;          /* Number of dependencies */
    xpdk_fd_t dependencies[16];         /* Dependency file descriptors */
};
```

### Primary Functions

#### Initialize Options
```c
void xpdk_open_opts_init(struct xpdk_open_opts *opts);
```
Initialize open options structure with sensible defaults.

#### Single Device Open
```c
int xpdk_open_advanced(const char *bdev_name, 
                       const struct xpdk_open_opts *opts,
                       struct xpdk_device_desc *desc);
```
Open a single device with advanced options and extended device information.

#### Batch Operations
```c
int xpdk_open_batch(const struct xpdk_open_request *requests, 
                    uint32_t count,
                    struct xpdk_open_result *result);
```
Open multiple devices in a single batch operation with dependency resolution.

#### Auto Resolution
```c
int xpdk_open_auto_resolve(const char **device_names, 
                           uint32_t count,
                           const struct xpdk_open_opts *opts,
                           struct xpdk_open_result *result);
```
Open multiple devices with automatic dependency discovery and resolution.

## Usage Examples

### Example 1: Simple Advanced Open

```c
#include "xpdk.h"
#include "xpdk_advanced.h"

int main() {
    // Initialize XPDK
    xpdk_init(NULL);
    
    // Configure advanced options
    struct xpdk_open_opts opts;
    xpdk_open_opts_init(&opts);
    opts.flags = O_RDWR;
    opts.enable_turbo = true;
    opts.cpu_core = 2;
    opts.queue_depth = 128;
    opts.timeout_ms = 10000;
    
    // Open device with advanced options
    struct xpdk_device_desc desc;
    int rc = xpdk_open_advanced("nvme0n1", &opts, &desc);
    
    if (rc == XPDK_SUCCESS) {
        printf("Device opened: %s (FD %d)\n", desc.name, desc.fd);
        printf("Capacity: %.2f GB\n", 
               (double)desc.info.capacity / (1024*1024*1024));
        
        // Use device...
        
        // Close device
        xpdk_close_advanced(&desc);
    }
    
    xpdk_cleanup();
    return 0;
}
```

### Example 2: Batch Device Opening

```c
int batch_open_example() {
    struct xpdk_open_request requests[3];
    struct xpdk_open_result result;
    
    // Initialize requests
    for (int i = 0; i < 3; i++) {
        xpdk_open_opts_init(&requests[i].opts);
        requests[i].opts.flags = O_RDWR;
        requests[i].opts.allow_partial_failure = true;
        requests[i].group_id = 1;
    }
    
    // Configure device names
    snprintf(requests[0].bdev_name, 256, "nvme0n1");
    snprintf(requests[1].bdev_name, 256, "nvme1n1");
    snprintf(requests[2].bdev_name, 256, "nvme2n1");
    
    // Open all devices in batch
    int rc = xpdk_open_batch(requests, 3, &result);
    
    printf("Batch result: %s\n", xpdk_strerror(rc));
    printf("Opened: %u, Failed: %u\n", 
           result.opened_count, result.failed_count);
    
    // Use opened devices...
    
    // Close all devices
    if (result.opened_count > 0) {
        xpdk_close_batch(result.devices, result.opened_count);
    }
    
    return rc;
}
```

### Example 3: Dependency Resolution

```c
int dependency_example() {
    struct xpdk_open_request requests[3];
    struct xpdk_open_result result;
    
    // Initialize base device (no dependencies)
    xpdk_open_opts_init(&requests[0].opts);
    snprintf(requests[0].bdev_name, 256, "base_device");
    requests[0].dependency_count = 0;
    
    // Initialize dependent device 1
    xpdk_open_opts_init(&requests[1].opts);
    snprintf(requests[1].bdev_name, 256, "dependent1");
    requests[1].dependency_count = 1;
    snprintf(requests[1].dependencies[0], 256, "base_device");
    
    // Initialize dependent device 2
    xpdk_open_opts_init(&requests[2].opts);
    snprintf(requests[2].bdev_name, 256, "dependent2");
    requests[2].dependency_count = 1;
    snprintf(requests[2].dependencies[0], 256, "dependent1");
    
    // Open with automatic dependency resolution
    int rc = xpdk_open_batch(requests, 3, &result);
    
    if (rc == XPDK_SUCCESS) {
        printf("Successfully resolved and opened all dependencies\n");
        
        // Validate dependencies
        for (uint32_t i = 0; i < result.opened_count; i++) {
            rc = xpdk_validate_dependencies(&result.devices[i]);
            printf("Device %s dependency validation: %s\n",
                   result.devices[i].name,
                   rc == XPDK_SUCCESS ? "PASSED" : "FAILED");
        }
        
        xpdk_close_batch(result.devices, result.opened_count);
    }
    
    return rc;
}
```

### Example 4: Performance Monitoring

```c
int performance_monitoring_example() {
    struct xpdk_open_opts opts;
    struct xpdk_device_desc desc;
    
    xpdk_open_opts_init(&opts);
    opts.flags = O_RDWR;
    opts.enable_turbo = true;
    
    int rc = xpdk_open_advanced("nvme0n1", &opts, &desc);
    if (rc != XPDK_SUCCESS) {
        return rc;
    }
    
    // Monitor device state
    xpdk_device_state_t state = xpdk_get_device_state(desc.fd);
    printf("Device state: %d\n", state);
    
    // Wait for device to be ready
    rc = xpdk_wait_device_state(desc.fd, XPDK_DEVICE_STATE_OPEN, 5000);
    if (rc == XPDK_SUCCESS) {
        printf("Device is ready for I/O\n");
        
        // Get performance statistics
        struct xpdk_perf_stats stats;
        rc = xpdk_get_device_stats(desc.fd, &stats);
        if (rc == XPDK_SUCCESS) {
            printf("Read Ops: %lu, Write Ops: %lu\n",
                   stats.total_read_ops, stats.total_write_ops);
            printf("Read Latency: %lu us, Write Latency: %lu us\n",
                   stats.avg_read_latency_us, stats.avg_write_latency_us);
        }
    }
    
    xpdk_close_advanced(&desc);
    return 0;
}
```

## Configuration Options

### Performance Tuning

```c
struct xpdk_open_opts opts;
xpdk_open_opts_init(&opts);

// High-performance configuration
opts.enable_turbo = true;           // Enable turbo mode
opts.cpu_core = 2;                  // Bind to specific CPU core
opts.queue_depth = 256;             // Large queue depth
opts.io_cache_size = 64;           // 64MB I/O cache

// QoS configuration
struct xpdk_qos_limits qos = {
    .limits = {
        [XPDK_QOS_RW_IOPS_RATE_LIMIT] = 100000,  // 100K IOPS
        [XPDK_QOS_RW_BPS_RATE_LIMIT] = 1000000000, // 1GB/s
    }
};
opts.qos_limits = &qos;
```

### Error Handling

```c
// Robust error handling configuration
opts.enable_auto_retry = true;      // Enable automatic retries
opts.max_retry_count = 5;           // Up to 5 retry attempts
opts.retry_delay_ms = 500;          // 500ms delay between retries
opts.timeout_ms = 30000;            // 30 second timeout
opts.allow_partial_failure = true;  // Allow partial success in batch ops
```

## Best Practices

### 1. Resource Management
- Always close devices when done using them
- Use batch operations for multiple devices
- Consider CPU affinity for performance-critical applications
- Monitor device states for robust applications

### 2. Error Handling
- Check return codes from all operations
- Use partial failure mode for non-critical devices
- Implement proper cleanup in error paths
- Log detailed error information for debugging

### 3. Performance Optimization
- Use turbo mode for high-performance scenarios
- Set appropriate queue depths based on workload
- Consider NUMA topology for multi-socket systems
- Monitor performance statistics to identify bottlenecks

### 4. Dependency Management
- Keep dependency chains simple when possible
- Validate dependencies before performing operations
- Use device groups for related devices
- Consider ordering when defining dependencies

## Building and Testing

### Enable Advanced Open

```bash
mkdir build && cd build
cmake -DENABLE_ADVANCED_OPEN=ON ..
make
```

### Run Tests

```bash
# Run all tests
make test

# Run specific advanced open tests
./tests/test_advanced_open
```

### Run Examples

```bash
# Simple advanced open example
./examples/advanced_open_example

# Performance comparison
./examples/turbo_benchmark
```

## Troubleshooting

### Common Issues

1. **Device not found**
   - Verify device name and availability
   - Check SPDK configuration
   - Ensure proper permissions

2. **Dependency resolution fails**
   - Check dependency chain for cycles
   - Verify all dependency devices exist
   - Use validation functions to debug

3. **Performance issues**
   - Check CPU affinity settings
   - Monitor queue depth utilization
   - Consider NUMA placement

4. **Timeout errors**
   - Increase timeout values
   - Check device health
   - Verify system resources

### Debug Mode

Enable debug mode for detailed logging:

```c
struct xpdk_opts xpdk_opts;
xpdk_opts_init(&xpdk_opts);
xpdk_opts.turbo_mode = false;  // Disable for debugging

// Add debug output
setenv("XPDK_DEBUG", "1", 1);
```

## Performance Considerations

### Scalability
- Batch operations scale better than individual opens
- Consider parallel initialization for large device counts
- Use appropriate timeout values for your environment

### Memory Usage
- Device descriptors consume memory
- Consider memory usage for large batch operations
- Monitor memory allocation patterns

### CPU Usage
- Turbo mode increases CPU usage
- CPU affinity can improve performance
- Balance performance vs. resource usage

## Migration from Basic API

### Simple Migration

Replace basic opens:
```c
// Before
xpdk_fd_t fd = xpdk_open("device", O_RDWR);

// After
struct xpdk_open_opts opts;
struct xpdk_device_desc desc;
xpdk_open_opts_init(&opts);
opts.flags = O_RDWR;
int rc = xpdk_open_advanced("device", &opts, &desc);
```

### Batch Migration

Convert multiple opens:
```c
// Before
xpdk_fd_t fd1 = xpdk_open("device1", O_RDWR);
xpdk_fd_t fd2 = xpdk_open("device2", O_RDWR);
xpdk_fd_t fd3 = xpdk_open("device3", O_RDWR);

// After
const char *devices[] = {"device1", "device2", "device3"};
struct xpdk_open_opts opts;
struct xpdk_open_result result;
xpdk_open_opts_init(&opts);
opts.flags = O_RDWR;
int rc = xpdk_open_auto_resolve(devices, 3, &opts, &result);
```

## Future Enhancements

- Hot-plug device support
- Dynamic load balancing
- Advanced caching strategies
- Encryption key management
- Cloud storage integration

## Support

For questions, issues, or feature requests:
- Check the examples directory for usage patterns
- Run the test suite to verify functionality
- Review performance optimization guidelines
- Consult the main README.md for general information
