# Quick Start Guide for libxpdk

This guide will help you get started with libxpdk quickly.

## Prerequisites

1. **SPDK Installation**: Ensure SPDK is installed on your system
   ```bash
   # Ubuntu/Debian
   sudo apt install libspdk-dev
   
   # Or build from source
   git clone https://github.com/spdk/spdk
   cd spdk
   ./scripts/pkgdep.sh
   ./configure
   make
   ```

2. **Development tools**:
   ```bash
   sudo apt install build-essential cmake pkg-config
   ```

## Building libxpdk

### Option 1: Using CMake (Recommended)
```bash
git clone <repository-url>
cd libxpdk
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Option 2: Using Make
```bash
git clone <repository-url>
cd libxpdk
make
```

## Running Examples

1. **List available devices**:
   ```bash
   ./build/examples/list_devices
   ```

2. **Simple I/O example**:
   ```bash
   ./build/examples/simple_example Nvme0n1
   ```

3. **Async I/O example**:
   ```bash
   ./build/examples/async_example Nvme0n1
   ```

4. **QoS (Quality of Service) example**:
   ```bash
   ./build/examples/qos_example
   ```

5. **Vectored I/O example**:
   ```bash
   ./build/examples/vectored_example Nvme0n1
   ```

6. **Batch I/O example**:
   ```bash
   ./build/examples/batch_example Nvme0n1
   ```

## Your First Program

Create a file called `hello_xpdk.c`:

```c
#include <stdio.h>
#include <fcntl.h>
#include "xpdk.h"

int main() {
    // Initialize the library
    if (xpdk_init(NULL) != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK\n");
        return 1;
    }
    
    // List available devices
    struct xpdk_bdev_info devices[10];
    int count = xpdk_list_bdevs(devices, 10);
    printf("Found %d devices\n", count);
    
    // Cleanup
    xpdk_cleanup();
    return 0;
}
```

Compile and run:
```bash
gcc -o hello_xpdk hello_xpdk.c -lxpdk $(pkg-config --cflags --libs spdk_bdev spdk_env_dpdk spdk_thread spdk_util)
./hello_xpdk
```

## Common Issues

### 1. SPDK Not Found
**Error**: `pkg-config: Package 'spdk_bdev' not found`

**Solution**: Make sure SPDK is installed and pkg-config can find it:
```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

### 2. Permission Issues
**Error**: Cannot access SPDK devices

**Solution**: Run with appropriate permissions:
```bash
sudo ./your_program
# Or add user to disk group
sudo usermod -a -G disk $USER
```

### 3. Huge Pages Not Configured
**Error**: SPDK initialization fails

**Solution**: Configure huge pages:
```bash
echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
```

## Next Steps

1. **Read the full documentation**: See `README.md`
2. **Study the examples**: Check `examples/` directory
3. **Run the tests**: `make test` or `ctest`
4. **Explore the API**: See `include/xpdk.h`

## Configuration

### SPDK Configuration File Example

Create `spdk.conf`:
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

Use it in your program:
```c
xpdk_init("spdk.conf");
```

## Performance Tips

1. **Use async I/O** for better performance
2. **Align I/O sizes** to block boundaries
3. **Batch operations** when possible
4. **Use proper buffer alignment** (handled automatically by libxpdk)
5. **Enable turbo mode** for latency-sensitive applications
6. **Use vectored I/O** for scatter-gather operations
7. **Monitor performance** with built-in statistics

## Advanced Features Examples

### Turbo Mode Example

Create a file called `turbo_test.c`:

```c
#include <stdio.h>
#include <fcntl.h>
#include "xpdk.h"

int main() {
    // Configure turbo mode
    struct xpdk_opts opts;
    xpdk_opts_init(&opts);
    opts.turbo_mode = true;
    opts.cpu_core = 1;          // Bind to CPU core 1
    
    // Initialize with turbo mode
    if (xpdk_init_opts(&opts) != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK with turbo mode\n");
        return 1;
    }
    
    printf("Turbo mode enabled with CPU core binding\n");
    
    // ... rest of your high-performance code ...
    
    xpdk_cleanup();
    return 0;
}
```

### Vectored I/O Example

Create a file called `vectored_test.c`:

```c
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include "xpdk.h"

int main() {
    xpdk_init(NULL);
    xpdk_fd_t fd = xpdk_open("Nvme0n1", O_RDWR);
    
    // Allocate aligned buffers
    char *buf1 = xpdk_alloc_buffer(4096);
    char *buf2 = xpdk_alloc_buffer(4096);
    char *buf3 = xpdk_alloc_buffer(4096);
    
    strcpy(buf1, "First chunk");
    strcpy(buf2, "Second chunk");
    strcpy(buf3, "Third chunk");
    
    // Setup vectored I/O
    struct xpdk_iovec iov[3] = {
        { .iov_base = buf1, .iov_len = 4096 },
        { .iov_base = buf2, .iov_len = 4096 },
        { .iov_base = buf3, .iov_len = 4096 }
    };
    
    // Write all buffers in one operation
    ssize_t written = xpdk_writev(fd, iov, 3, 0);
    printf("Vectored write: %zd bytes\n", written);
    
    // Cleanup
    xpdk_free_buffer(buf1);
    xpdk_free_buffer(buf2);
    xpdk_free_buffer(buf3);
    xpdk_close(fd);
    xpdk_cleanup();
    return 0;
}
```

### Batch I/O Example

Create a file called `batch_test.c`:

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "xpdk.h"

static volatile int completed = 0;

void callback(void *ctx, int status) {
    int *op_id = (int *)ctx;
    printf("Operation %d completed: %s\n", *op_id, xpdk_strerror(status));
    __sync_fetch_and_add(&completed, 1);
}

int main() {
    xpdk_init(NULL);
    xpdk_fd_t fd = xpdk_open("Nvme0n1", O_RDWR);
    
    // Initialize batch context
    struct xpdk_batch_ctx *batch = xpdk_batch_init(8);
    
    // Prepare batch operations
    struct xpdk_batch_io ios[4];
    char *buffers[4];
    int op_ids[4];
    
    for (int i = 0; i < 4; i++) {
        buffers[i] = xpdk_alloc_buffer(4096);
        snprintf(buffers[i], 4096, "Batch operation %d", i);
        op_ids[i] = i;
        
        ios[i].type = XPDK_IO_WRITE;
        ios[i].fd = fd;
        ios[i].buffer = buffers[i];
        ios[i].count = 4096;
        ios[i].offset = i * 4096;
        ios[i].callback = callback;
        ios[i].ctx = &op_ids[i];
    }
    
    // Submit batch
    xpdk_batch_submit(batch, ios, 4, NULL);
    
    // Wait for completion
    while (completed < 4) {
        xpdk_poll(10);
        usleep(1000);
    }
    
    // Cleanup
    xpdk_batch_cleanup(batch);
    for (int i = 0; i < 4; i++) {
        xpdk_free_buffer(buffers[i]);
    }
    xpdk_close(fd);
    xpdk_cleanup();
    return 0;
}
```

### Performance Statistics Example

Create a file called `stats_test.c`:

```c
#include <stdio.h>
#include <fcntl.h>
#include "xpdk.h"

int main() {
    xpdk_init(NULL);
    xpdk_fd_t fd = xpdk_open("Nvme0n1", O_RDWR);
    
    // Reset statistics
    xpdk_reset_perf_stats(fd);
    
    // Perform some I/O
    char *buffer = xpdk_alloc_buffer(4096);
    for (int i = 0; i < 100; i++) {
        xpdk_write(fd, buffer, 4096, i * 4096);
    }
    
    // Get statistics
    struct xpdk_perf_stats stats;
    if (xpdk_get_perf_stats(fd, &stats) == XPDK_SUCCESS) {
        printf("Performance Statistics:\n");
        printf("  Total ops: %lu\n", stats.total_ops);
        printf("  Total bytes: %lu\n", stats.total_bytes);
        printf("  Average latency: %.2f μs\n", stats.avg_latency_us);
        printf("  Current IOPS: %.2f\n", stats.current_iops);
        printf("  Current bandwidth: %.2f MB/s\n", stats.current_bps / (1024.0 * 1024.0));
    }
    
    xpdk_free_buffer(buffer);
    xpdk_close(fd);
    xpdk_cleanup();
    return 0;
}
```

## QoS (Quality of Service) Example

Create a file called `qos_test.c` to demonstrate QoS functionality:

```c
#include <stdio.h>
#include <fcntl.h>
#include "xpdk.h"

int main() {
    // Initialize the library
    if (xpdk_init(NULL) != XPDK_SUCCESS) {
        printf("Failed to initialize XPDK\n");
        return 1;
    }
    
    // Open a device
    xpdk_fd_t fd = xpdk_open("Nvme0n1", O_RDWR);
    if (fd < 0) {
        printf("Failed to open device\n");
        xpdk_cleanup();
        return 1;
    }
    
    // Set QoS limits: 1000 IOPS, 4MB/s bandwidth
    printf("Setting QoS limits: 1000 IOPS, 4MB/s\n");
    if (xpdk_qos_enable_simple(fd, 1000, 4 * 1024 * 1024) == XPDK_SUCCESS) {
        printf("QoS enabled successfully\n");
    }
    
    // Perform some I/O operations (will be rate-limited)
    char buffer[4096] = "Test data with QoS";
    ssize_t result = xpdk_write(fd, buffer, sizeof(buffer), 0);
    printf("Write completed: %zd bytes\n", result);
    
    // Disable QoS
    xpdk_qos_disable(fd);
    printf("QoS disabled\n");
    
    // Cleanup
    xpdk_close(fd);
    xpdk_cleanup();
    return 0;
}
```

Happy coding with libxpdk!
