#!/bin/bash

# RAID1测试自动化脚本
# 自动设置环境、编译并运行RAID1测试

set -e  # 遇到错误立即退出

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "🚀 libxpdk RAID1 自动化测试脚本"
echo "================================="
echo "项目根目录: $PROJECT_ROOT"
echo "测试目录: $SCRIPT_DIR"
echo ""

# 检查是否为root用户（SPDK需要root权限）
if [[ $EUID -ne 0 ]]; then
   echo "❌ 此脚本需要root权限运行SPDK"
   echo "   请使用: sudo $0"
   exit 1
fi

# 进入测试目录
cd "$SCRIPT_DIR"

# 函数：配置系统环境
setup_environment() {
    echo "🔧 配置系统环境..."
    
    # 配置hugepage
    echo "配置hugepage (1024 pages)..."
    echo 1024 > /proc/sys/vm/nr_hugepages
    
    # 挂载hugepage
    echo "挂载hugepage文件系统..."
    mkdir -p /mnt/huge
    if ! mountpoint -q /mnt/huge; then
        mount -t hugetlbfs nodev /mnt/huge
    fi
    
    # 设置库路径
    export LD_LIBRARY_PATH="$PROJECT_ROOT/build:$PROJECT_ROOT/src/spdk/build/lib:$PROJECT_ROOT/src/spdk/dpdk/build/lib:$LD_LIBRARY_PATH"
    echo "库路径已设置: $LD_LIBRARY_PATH"
    
    echo "✅ 系统环境配置完成"
}

# 函数：创建测试文件
setup_test_files() {
    echo "📁 创建测试文件..."
    
    if [[ ! -f "disk1.img" ]]; then
        echo "创建 disk1.img (500MB)..."
        dd if=/dev/zero of=disk1.img bs=1M count=500 status=progress
    else
        echo "disk1.img 已存在，跳过创建"
    fi
    
    if [[ ! -f "disk2.img" ]]; then
        echo "创建 disk2.img (500MB)..."
        dd if=/dev/zero of=disk2.img bs=1M count=500 status=progress
    else
        echo "disk2.img 已存在，跳过创建"
    fi
    
    echo "✅ 测试文件创建完成"
}

# 函数：编译测试程序
build_test() {
    echo "🔨 编译测试程序..."
    
    # 检查libxpdk是否已编译
    if [[ ! -f "$PROJECT_ROOT/build/libxpdk.so" ]]; then
        echo "libxpdk库未找到，先编译libxpdk..."
        cd "$PROJECT_ROOT/build"
        make -j$(nproc)
        cd "$SCRIPT_DIR"
    fi
    
    # 编译测试程序
    make clean
    make all
    
    echo "✅ 测试程序编译完成"
}

# 函数：运行测试
run_test() {
    echo "🧪 运行RAID1测试..."
    
    # 检查测试程序是否存在
    if [[ ! -f "raid1_test" ]]; then
        echo "❌ 测试程序不存在，请先编译"
        exit 1
    fi
    
    # 检查配置文件
    if [[ ! -f "spdk_raid1.conf" ]]; then
        echo "❌ SPDK配置文件不存在"
        exit 1
    fi
    
    echo "开始执行测试..."
    echo "========================"
    
    # 运行测试程序
    if ./raid1_test; then
        echo "========================"
        echo "✅ 测试成功完成！"
    else
        echo "========================"
        echo "❌ 测试失败！"
        exit 1
    fi
}

# 函数：清理环境
cleanup() {
    echo "🧹 清理测试环境..."
    make clean
    echo "✅ 清理完成"
}

# 函数：显示帮助信息
show_help() {
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  setup    - 仅设置环境和创建测试文件"
    echo "  build    - 仅编译测试程序"
    echo "  test     - 仅运行测试(需要先编译)"
    echo "  clean    - 清理编译文件"
    echo "  all      - 完整测试流程(默认)"
    echo "  help     - 显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  sudo $0           # 运行完整测试"
    echo "  sudo $0 setup     # 仅准备环境"
    echo "  sudo $0 build     # 仅编译"
    echo "  sudo $0 test      # 仅测试"
}

# 主逻辑
case "${1:-all}" in
    setup)
        setup_environment
        setup_test_files
        ;;
    build)
        setup_environment
        build_test
        ;;
    test)
        setup_environment
        run_test
        ;;
    clean)
        cleanup
        ;;
    all)
        setup_environment
        setup_test_files
        build_test
        run_test
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        echo "❌ 未知选项: $1"
        show_help
        exit 1
        ;;
esac

echo ""
echo "🎉 脚本执行完成！"
