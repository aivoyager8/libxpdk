#!/bin/bash

# libxpdk运行环境配置脚本

echo "配置libxpdk运行环境..."

# 设置库路径
export LD_LIBRARY_PATH="/home/libxpdk/src/spdk/build/lib:/home/libxpdk/src/spdk/dpdk/build/lib:/home/libxpdk/build:$LD_LIBRARY_PATH"

# 设置hugepage (需要root权限)
echo "配置hugepage..."
echo 1024 > /proc/sys/vm/nr_hugepages

# 挂载hugepage
mkdir -p /mnt/huge
mount -t hugetlbfs nodev /mnt/huge

echo "环境配置完成！"
echo "库路径: $LD_LIBRARY_PATH"
echo "可以运行示例程序了。"

# 显示使用说明
echo ""
echo "使用方法："
echo "  source run_env.sh"
echo "  cd build"
echo "  ./examples/simple_example"

