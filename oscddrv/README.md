# oscddrv — Linux 内核驱动模块

`oscddrv` 是 OSCD 项目中的内核驱动模块，运行在内核态。它一方面作为实验四的基础框架，另一方面承担调度测试引擎的角色，与实验一（`oscdsh`）联动，通过 `ioctl` 接口接收用户态命令并执行测试任务。

## 快速开始

### 编译

```bash
cd oscd/oscddrv
make CC=gcc-12
```

> 如果系统默认 GCC 版本与编译内核时使用的版本一致，可直接使用 `make`。若出现编译器版本警告，请指定正确的版本（如 `CC=gcc-12`）。

### 加载驱动

```bash
sudo insmod oscddrv.ko
```

### 查看加载日志

```bash
sudo dmesg | tail -20
```

预期输出中应包含类似以下内容：

```
[ 1234.567890] oscddrv: 驱动程序加载成功！(版本 0.1)
```

### 卸载驱动

```bash
sudo rmmod oscddrv
```

再次执行 `sudo dmesg | tail -20`，预期输出：

```
[ 1234.678901] oscddrv: 驱动程序卸载成功！
```

## 目录结构

```
oscd/oscddrv/
├── Makefile
├── README.md
├── include/
│   └── driver.h          # 公共头文件（驱动名称、版本、函数声明）
└── src/
    └── main.c            # 驱动主程序（module_init / module_exit）
```

## 当前状态

- [x] 模块加载与卸载（`insmod` / `rmmod`）
- [x] 内核日志输出（`printk`）
- [ ] 设备号分配（`alloc_chrdev_region`）
- [ ] 字符设备注册（`cdev_init`、`cdev_add`）
- [ ] 自动创建设备文件（`class_create`、`device_create`）
- [ ] `open` / `release` 回调
- [ ] `read` / `write` 回调（内核缓冲区）
- [ ] 多进程独立偏移（`file->private_data`）
- [ ] `ioctl` 控制接口
- [ ] 与 `oscdsh` 联动命令（`driver write/read/status`）
- [ ] 调度测试引擎（`sched_test` 系列）

## 设计目标

- **内核态运行**：作为内核模块动态加载，无需重新编译内核
- **虚拟字符设备**：提供 `/dev/oscddrv` 设备节点，支持用户态 `open` / `read` / `write`
- **多进程支持**：每个进程打开设备拥有独立的读写偏移
- **测试引擎**：通过 `ioctl` 接收调度测试命令，在内核态创建测试任务
- **与 Shell 联动**：`oscdsh` 通过设备文件与驱动通信，实现“测试 → 监控 → 存储”闭环

## 注意事项

- **内核头文件**：编译前需安装与当前内核版本对应的头文件：
  ```bash
  sudo apt install linux-headers-$(uname -r)
  ```
- **编译器版本**：确保使用的 GCC 版本与编译内核时相同，否则可能出现警告或错误。可用以下命令查看内核编译所用的 GCC 版本：
  ```bash
  cat /proc/version
  ```
- **日志查看**：驱动中的 `printk` 输出通过 `dmesg` 查看，使用 `sudo dmesg -w` 可实时跟踪日志。
- **模块卸载**：如果驱动正在被使用（如设备文件被占用），`rmmod` 可能失败。请确保所有引用驱动的进程都已关闭。

## 作者

操作系统课程设计（OSCD）小组