# RAID1 测试环境

本目录包含用于测试libxpdk与SPDK RAID1集成的完整测试环境。

## 📁 文件说明

- **spdk_raid1.conf** - SPDK配置文件，定义RAID1设备
- **raid1_test.c** - 主测试程序源代码
- **Makefile** - 编译配置文件
- **run_test.sh** - 自动化测试脚本
- **disk1.img, disk2.img** - 500MB测试文件（运行时创建）

## 🚀 快速开始

### 自动化测试（推荐）

```bash
# 运行完整测试（需要root权限）
sudo ./run_test.sh

# 或者分步执行
sudo ./run_test.sh setup   # 准备环境
sudo ./run_test.sh build   # 编译程序
sudo ./run_test.sh test    # 运行测试
```

### 手动测试

```bash
# 1. 配置环境
source ../run_env.sh

# 2. 创建测试文件
make setup

# 3. 编译测试程序
make all

# 4. 运行测试
make test
```

## 🧪 测试内容

### 基本功能测试
- RAID1设备打开/关闭
- 基本读写操作
- 数据完整性验证
- 设备信息获取

### 性能测试
- 读写延迟测试
- 吞吐量测试
- 1MB数据的完整性验证

### RAID1特性测试
- 镜像写入验证
- 故障容错能力
- 数据一致性检查

## 📊 预期结果

成功的测试应该显示：
- ✅ 基本读写测试通过
- ✅ 数据完整性验证通过
- 📈 性能统计数据
- 🎉 所有测试完成

## 🔧 环境要求

- **操作系统**: Linux (Ubuntu/CentOS)
- **权限**: Root权限（SPDK要求）
- **内存**: 至少2GB可用内存
- **Hugepage**: 1024页面（约2GB）

## 📝 配置说明

### SPDK配置 (spdk_raid1.conf)
```ini
[Aio]
  # 两个500MB文件作为后端存储
  AIO /path/to/disk1.img Aio0 4096
  AIO /path/to/disk2.img Aio1 4096

[Raid]
  # RAID1配置
  Name Raid1
  RaidLevel 1
  Devices Aio0 Aio1
```

### 编译选项
- **标准版本**: `make all`
- **调试版本**: `make debug`
- **发布版本**: `make release`

## 🛠️ 故障排除

### 常见问题

1. **权限错误**
   ```bash
   sudo ./run_test.sh
   ```

2. **Hugepage不足**
   ```bash
   echo 1024 > /proc/sys/vm/nr_hugepages
   ```

3. **库路径问题**
   ```bash
   source ../run_env.sh
   ```

4. **SPDK初始化失败**
   - 检查配置文件路径
   - 验证磁盘文件存在
   - 确认hugepage已挂载

### 调试模式

```bash
# 编译调试版本
make debug

# 查看详细日志
export SPDK_LOG_LEVEL=DEBUG
./raid1_test
```

## 📈 性能基准

在典型环境下的预期性能：
- **读取延迟**: < 100 微秒
- **写入延迟**: < 200 微秒  
- **读取吞吐量**: > 100 MB/s
- **写入吞吐量**: > 50 MB/s

实际性能取决于硬件配置和系统负载。

## 🔗 相关文档

- [libxpdk API文档](../README.md)
- [SPDK文档](../src/spdk/doc/)
- [构建说明](../BUILD_SUCCESS.md)
- [开发流程](../DEVELOPMENT_WORKFLOW.md)

---

**注意**: 此测试环境仅用于开发和验证目的，不应在生产环境中使用。
