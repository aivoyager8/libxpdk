# libxpdk 远程编译指令

## 连接服务器
ssh root@192.168.13.128

## 创建工作目录
mkdir -p /home/libxpdk
cd /home/libxpdk

## 如果需要从本地传输代码
# 在本地执行：
# scp -r /path/to/libxpdk root@192.168.13.128:/home/libxpdk/

## 或者从Git拉取代码（如果有远程仓库）
# git clone <your-repo-url> .

## 安装编译依赖（CentOS/RHEL）
yum update -y
yum groupinstall -y "Development Tools"
yum install -y cmake gcc gcc-c++ make git pkg-config
yum install -y libuuid-devel openssl-devel libaio-devel
yum install -y numactl-devel

## 或者Ubuntu/Debian
# apt-get update
# apt-get install -y build-essential cmake gcc g++ make git pkg-config
# apt-get install -y uuid-dev libssl-dev libaio-dev libnuma-dev

## 编译项目
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

## 检查编译结果
ls -la *.so *.a 2>/dev/null || echo "库文件生成检查"
ls -la examples/ 2>/dev/null || echo "示例程序检查"

## 运行测试
make test 2>/dev/null || echo "测试需要先配置SPDK环境"
