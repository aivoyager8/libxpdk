#!/bin/bash

# libxpdk 编译服务器配置脚本
# 用于配置远程编译服务器的SSH免密登录和环境

set -e

# 配置参数
BUILD_SERVER="192.168.13.128"
BUILD_USER="root"
PROJECT_NAME="libxpdk"
REMOTE_DIR="/opt/libxpdk"

echo "=========================================="
echo "libxpdk 编译服务器配置脚本"
echo "服务器: ${BUILD_SERVER}"
echo "用户: ${BUILD_USER}"
echo "=========================================="

# 函数：检查SSH密钥
check_ssh_key() {
    if [ ! -f ~/.ssh/id_rsa ]; then
        echo "正在生成SSH密钥对..."
        ssh-keygen -t rsa -b 4096 -f ~/.ssh/id_rsa -N ""
        echo "SSH密钥生成完成"
    else
        echo "SSH密钥已存在"
    fi
}

# 函数：配置免密登录
setup_passwordless_login() {
    echo "配置SSH免密登录..."
    echo "请输入服务器密码来复制公钥："
    
    # 复制公钥到远程服务器
    ssh-copy-id -i ~/.ssh/id_rsa.pub ${BUILD_USER}@${BUILD_SERVER}
    
    if [ $? -eq 0 ]; then
        echo "SSH免密登录配置成功！"
    else
        echo "SSH免密登录配置失败！"
        exit 1
    fi
}

# 函数：测试连接
test_connection() {
    echo "测试SSH连接..."
    ssh -o BatchMode=yes -o ConnectTimeout=5 ${BUILD_USER}@${BUILD_SERVER} "echo 'SSH连接成功！'"
    
    if [ $? -eq 0 ]; then
        echo "SSH免密登录验证成功！"
    else
        echo "SSH连接失败，请检查配置"
        exit 1
    fi
}

# 函数：安装编译依赖
install_build_dependencies() {
    echo "安装编译依赖..."
    ssh ${BUILD_USER}@${BUILD_SERVER} << 'EOF'
        # 检测操作系统
        if [ -f /etc/redhat-release ]; then
            # CentOS/RHEL
            echo "检测到 CentOS/RHEL 系统"
            yum update -y
            yum groupinstall -y "Development Tools"
            yum install -y cmake gcc gcc-c++ make git pkg-config
            yum install -y libuuid-devel openssl-devel libaio-devel
            yum install -y numactl-devel libfabric-devel
        elif [ -f /etc/debian_version ]; then
            # Ubuntu/Debian
            echo "检测到 Ubuntu/Debian 系统"
            apt-get update
            apt-get install -y build-essential cmake gcc g++ make git pkg-config
            apt-get install -y uuid-dev libssl-dev libaio-dev
            apt-get install -y libnuma-dev libfabric-dev
        else
            echo "不支持的操作系统"
            exit 1
        fi
        
        echo "编译依赖安装完成"
EOF
}

# 函数：创建项目目录
setup_project_directory() {
    echo "创建项目目录..."
    ssh ${BUILD_USER}@${BUILD_SERVER} << EOF
        mkdir -p ${REMOTE_DIR}
        cd ${REMOTE_DIR}
        echo "项目目录创建完成: ${REMOTE_DIR}"
EOF
}

# 函数：同步代码到服务器
sync_code() {
    echo "同步代码到编译服务器..."
    
    # 排除不需要的文件和目录
    rsync -avz --progress \
        --exclude='.git' \
        --exclude='build/' \
        --exclude='*.o' \
        --exclude='*.so' \
        --exclude='*.a' \
        --exclude='CMakeCache.txt' \
        --exclude='CMakeFiles/' \
        . ${BUILD_USER}@${BUILD_SERVER}:${REMOTE_DIR}/
    
    echo "代码同步完成"
}

# 函数：远程编译
remote_build() {
    echo "开始远程编译..."
    ssh ${BUILD_USER}@${BUILD_SERVER} << EOF
        cd ${REMOTE_DIR}
        
        # 创建构建目录
        mkdir -p build
        cd build
        
        # 配置CMake
        echo "配置CMake..."
        cmake .. -DCMAKE_BUILD_TYPE=Release
        
        # 编译
        echo "开始编译..."
        make -j\$(nproc)
        
        if [ \$? -eq 0 ]; then
            echo "编译成功！"
            echo "生成的文件："
            ls -la *.so *.a 2>/dev/null || true
        else
            echo "编译失败！"
            exit 1
        fi
EOF
}

# 函数：创建编译脚本
create_build_script() {
    echo "创建远程编译脚本..."
    
    cat > build_remote.sh << 'EOF'
#!/bin/bash
# libxpdk 远程编译脚本

BUILD_SERVER="192.168.13.128"
BUILD_USER="root"
REMOTE_DIR="/opt/libxpdk"

echo "同步代码到编译服务器..."
rsync -avz --progress \
    --exclude='.git' \
    --exclude='build/' \
    --exclude='*.o' \
    --exclude='*.so' \
    --exclude='*.a' \
    --exclude='CMakeCache.txt' \
    --exclude='CMakeFiles/' \
    . ${BUILD_USER}@${BUILD_SERVER}:${REMOTE_DIR}/

echo "开始远程编译..."
ssh ${BUILD_USER}@${BUILD_SERVER} << 'REMOTE_EOF'
    cd /opt/libxpdk
    mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    echo "编译完成，生成的文件："
    ls -la *.so *.a 2>/dev/null || true
REMOTE_EOF

echo "远程编译完成！"
EOF

    chmod +x build_remote.sh
    echo "远程编译脚本已创建: build_remote.sh"
}

# 主执行流程
main() {
    echo "开始配置编译服务器..."
    
    # 1. 检查并生成SSH密钥
    check_ssh_key
    
    # 2. 配置免密登录
    setup_passwordless_login
    
    # 3. 测试连接
    test_connection
    
    # 4. 安装编译依赖
    echo "是否安装编译依赖？(y/n)"
    read -r install_deps
    if [ "$install_deps" = "y" ] || [ "$install_deps" = "Y" ]; then
        install_build_dependencies
    fi
    
    # 5. 创建项目目录
    setup_project_directory
    
    # 6. 同步代码
    echo "是否立即同步代码？(y/n)"
    read -r sync_now
    if [ "$sync_now" = "y" ] || [ "$sync_now" = "Y" ]; then
        sync_code
    fi
    
    # 7. 是否立即编译
    echo "是否立即进行编译测试？(y/n)"
    read -r build_now
    if [ "$build_now" = "y" ] || [ "$build_now" = "Y" ]; then
        remote_build
    fi
    
    # 8. 创建便捷的编译脚本
    create_build_script
    
    echo ""
    echo "=========================================="
    echo "编译服务器配置完成！"
    echo ""
    echo "使用方法："
    echo "1. 运行 ./build_remote.sh 进行远程编译"
    echo "2. 直接SSH登录: ssh ${BUILD_USER}@${BUILD_SERVER}"
    echo "3. 项目目录: ${REMOTE_DIR}"
    echo "=========================================="
}

# 执行主函数
main "$@"
