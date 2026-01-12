# RAID1测试Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
INCLUDES = -I../include -I../src/spdk/include -I../src/spdk/dpdk/build/include
LIBS = -L../build -L../src/spdk/build/lib -L../src/spdk/dpdk/build/lib

# SPDK库链接顺序很重要
SPDK_LIBS = -lspdk_bdev -lspdk_bdev_malloc -lspdk_bdev_aio -lspdk_bdev_raid \
            -lspdk_thread -lspdk_util -lspdk_env_dpdk -lspdk_log \
            -lspdk_json -lspdk_jsonrpc -lspdk_rpc -lspdk_conf

# DPDK库
DPDK_LIBS = -lrte_eal -lrte_mempool -lrte_ring -lrte_mbuf -lrte_bus_pci \
            -lrte_pci -lrte_mempool_ring -lrte_telemetry

# 系统库
SYS_LIBS = -lpthread -lrt -luuid -lssl -lcrypto -lm -ldl -lnuma

# libxpdk库
XPDK_LIBS = -lxpdk

# 所有库
ALL_LIBS = $(XPDK_LIBS) $(SPDK_LIBS) $(DPDK_LIBS) $(SYS_LIBS)

TARGET = raid1_test
SOURCE = raid1_test.c

.PHONY: all clean test setup

all: $(TARGET)

$(TARGET): $(SOURCE)
	@echo "编译RAID1测试程序..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LIBS) $(ALL_LIBS)
	@echo "编译完成: $@"

clean:
	@echo "清理编译文件..."
	rm -f $(TARGET)

setup:
	@echo "创建RAID1测试环境..."
	@echo "创建500MB测试文件..."
	dd if=/dev/zero of=disk1.img bs=1M count=500
	dd if=/dev/zero of=disk2.img bs=1M count=500
	@echo "测试文件创建完成!"

test: $(TARGET) setup
	@echo "配置运行环境..."
	@# 确保有足够的hugepage
	@echo 1024 | sudo tee /proc/sys/vm/nr_hugepages > /dev/null || true
	@# 挂载hugepage
	@sudo mkdir -p /mnt/huge || true
	@sudo mount -t hugetlbfs nodev /mnt/huge || true
	@echo "运行RAID1测试..."
	@export LD_LIBRARY_PATH="../build:../src/spdk/build/lib:../src/spdk/dpdk/build/lib:$$LD_LIBRARY_PATH" && ./$(TARGET)

# 调试版本
debug: CFLAGS += -DDEBUG -O0
debug: $(TARGET)

# 发布版本
release: CFLAGS += -DNDEBUG -O3
release: $(TARGET)

# 调试程序
debug_memory: debug_memory.c
	@echo "编译内存调试程序..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LIBS) $(SPDK_LIBS) $(DPDK_LIBS) $(SYS_LIBS)
	@echo "编译完成: $@"

help:
	@echo "可用目标:"
	@echo "  all     - 编译测试程序"
	@echo "  clean   - 清理编译文件"
	@echo "  setup   - 创建测试环境(磁盘文件)"
	@echo "  test    - 编译并运行测试"
	@echo "  debug   - 编译调试版本"
	@echo "  release - 编译发布版本"
	@echo "  help    - 显示此帮助信息"
