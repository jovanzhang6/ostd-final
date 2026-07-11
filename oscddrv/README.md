# oscddrv — Linux 内核驱动模块

`oscddrv` 是 OSCD 项目中的内核驱动模块，运行在内核态。它一方面作为实验四的基础框架，实现了一个虚拟字符设备驱动，另一方面预留了调度测试引擎接口，后续可与实验一（`oscdsh`）联动，通过 `ioctl` 接收用户态命令并执行测试任务。

## 快速开始

### 编译

```bash
cd oscd/oscddrv
make
```

> 若系统 GCC 版本与内核编译版本不一致，可指定 `CC=gcc-12` 等。

### 加载驱动

```bash
make install
# 或 sudo insmod oscddrv.ko
```

### 查看加载日志

```bash
make info
# 或 sudo dmesg | tail -20
```

预期输出中包含：

```
oscddrv: allocated device number (major=..., minor=0)
oscddrv: module loaded successfully
```

### 运行完整测试

```bash
# 编译驱动并加载，然后按 test.txt 逐行验证
make
make install
sudo chmod 666 /dev/oscddrv
# 参照 test.txt 执行后续测试
```

### 卸载驱动

```bash
make remove
# 或 sudo rmmod oscddrv
```

查看日志应包含：

```
oscddrv: module unloaded successfully
```

## 目录结构

```
oscd/oscddrv/
├── Makefile
├── README.md
├── test.txt               # 全功能逐步验证脚本
├── test.c                 # ioctl 接口测试程序（编译生成 test 可执行文件）
├── include/
│   └── oscddrv.h          # 公共头文件（所有 include、宏、数据结构、ioctl 命令）
└── src/
    └── main.c             # 驱动主程序（所有回调、全局变量、ioctl 实现）
```

## 已实现功能

### 模块基础框架
- `module_init` / `module_exit` 入口
- 加载/卸载日志输出

### 字符设备注册与自动节点
- 动态分配设备号（`alloc_chrdev_region`）
- 字符设备注册（`cdev_init` + `cdev_add`）
- 自动创建 `/dev/oscddrv`（`class_create` + `device_create`）
- 模块卸载时自动销毁节点并归还设备号

### 设备打开与关闭（open / release）
- `open` 时分配私有数据结构（`struct oscddrv_private`），初始化偏移与计数
- `release` 时释放私有数据
- 多进程可同时打开，每个文件描述符拥有独立上下文

### 基本读写功能（read / write）
- **全局缓冲区**：4KB（可动态调整），模块加载时分配，卸载时释放
- `write`：从用户空间拷贝数据到全局缓冲区，更新偏移和写入计数
- `read`：从全局缓冲区拷贝数据到用户空间，更新偏移和读取计数
- 写入满时返回 `-ENOSPC`，读完返回 0（EOF）

### 多进程独立偏移支持
- 每次 `open` 分配独立 `offset`，读写操作仅影响自身偏移
- 多个进程同时读写同一设备，偏移互不干扰

### ioctl 增强控制接口
- **CMD_GET_STATS**：获取全局统计信息（缓冲区大小、累计读写次数、工作模式）
- **CMD_RESET**：清空全局缓冲区，重置全局读写计数
- **CMD_SET_BUFSIZE**：动态调整全局缓冲区大小（上限 65536 字节）
- **CMD_SET_MODE**：切换工作模式（normal / performance）
- 未知命令返回 `-ENOTTY`

## 测试与验证

- **基础读写**：`echo "hello" > /dev/oscddrv` 和 `cat /dev/oscddrv` 正常工作
- **多进程偏移**：两个终端同时打开设备，各自偏移独立，互不影响
- **动态缓冲**：`test.c` 可验证缓冲区大小调整、模式切换、统计重置等 ioctl 命令
- **完整测试**：`test.txt` 提供从加载到卸载的全流程脚本，覆盖所有功能

## 设计目标

- **内核态运行**：作为内核模块动态加载，无需重新编译内核
- **虚拟字符设备**：提供 `/dev/oscddrv` 设备节点，支持用户态 `open` / `read` / `write`
- **多进程支持**：每个进程打开设备拥有独立的读写偏移
- **可控制**：通过 `ioctl` 查询状态、重置驱动、调整缓冲区、切换模式
- **可扩展**：预留调度测试引擎接口（`CMD_SCHED_*`），未来可与实验一联动

## 注意事项

- **内核头文件**：编译前需安装与当前内核版本对应的头文件：
  ```bash
  sudo apt install linux-headers-$(uname -r)
  ```
- **编译器版本**：确保使用的 GCC 版本与编译内核时相同，否则可能出现警告或错误。可用以下命令查看内核编译所用的 GCC 版本：
  ```bash
  cat /proc/version
  ```
- **日志查看**：驱动中的 `printk` 输出通过 `dmesg` 查看，使用 `sudo dmesg -w` 可实时跟踪日志。亦可用 `make info` 快速查看最近日志。
- **权限**：设备节点默认仅 root 可读写，测试时需 `sudo` 或 `chmod 666 /dev/oscddrv`。
- **模块卸载**：如果驱动正在被使用（如设备文件被占用），`rmmod` 可能失败。请确保所有引用驱动的进程都已关闭。

## 作者

操作系统课程设计小组  
2026.07