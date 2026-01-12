#!/bin/bash

# libxpdk 自动连接和编译脚本
# 使用方法: ./connect_and_build.sh

SERVER="192.168.13.128"
USER="root"
REMOTE_DIR="/home/libxpdk"

echo "=========================================="
echo "连接到编译服务器: ${SERVER}"
echo "=========================================="

# 检查连接
echo "测试SSH连接..."
ssh -o ConnectTimeout=5 ${USER}@${SERVER} "echo '连接成功!'"

if [ $? -ne 0 ]; then
    echo "无法连接到服务器，请检查："
    echo "1. 网络连接"
    echo "2. SSH服务是否启动"
    echo "3. 防火墙设置"
    echo "4. 用户权限"
    exit 1
fi

echo "=========================================="
echo "同步代码到服务器..."
echo "=========================================="

# 同步代码
rsync -avz --progress \
    --exclude='.git' \
    --exclude='build/' \
    --exclude='*.o' \
    --exclude='*.so' \
    --exclude='*.a' \
    --exclude='CMakeCache.txt' \
    --exclude='CMakeFiles/' \
    . ${USER}@${SERVER}:${REMOTE_DIR}/

echo "=========================================="
echo "在服务器上编译项目..."
echo "=========================================="

# 远程编译
ssh ${USER}@${SERVER} << 'EOF'
set -e

cd /home/libxpdk

echo "当前目录: $(pwd)"
echo "项目文件:"
ls -la

# 检查是否需要安装依赖
if ! command -v cmake &> /dev/null; then
    echo "安装编译依赖..."
    if [ -f /etc/redhat-release ]; then
        yum update -y
        yum groupinstall -y "Development Tools"
        yum install -y cmake gcc gcc-c++ make git pkg-config
        yum install -y libuuid-devel openssl-devel libaio-devel numactl-devel
    elif [ -f /etc/debian_version ]; then
        apt-get update
        apt-get install -y build-essential cmake gcc g++ make git pkg-config
        apt-get install -y uuid-dev libssl-dev libaio-dev libnuma-dev
    fi
fi

echo "创建构建目录..."
mkdir -p build
cd build

echo "配置CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

echo "开始编译..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo "=========================================="
    echo "编译成功！"
    echo "=========================================="
    echo "生成的库文件:"
    ls -la *.so *.a 2>/dev/null || echo "没有找到库文件"
    echo ""
    echo "示例程序:"
    ls -la examples/ 2>/dev/null || echo "没有找到示例程序"
    echo ""
    echo "编译完成！项目位于: /home/libxpdk"
else
    echo "编译失败！请检查错误信息"
    exit 1
fi
EOF

echo "=========================================="
echo "编译完成！"
echo "连接到服务器查看结果:"
echo "ssh ${USER}@${SERVER}"
echo "cd ${REMOTE_DIR}"
echo "=========================================="
