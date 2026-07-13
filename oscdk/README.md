# oscdk — Linux 内核实验（实验三）

本目录包含实验三（系统调用）相关的内核源码修改、用户态测试程序和补丁记录。

## 目录结构

```
oscdk/
├── linux-hwe-6.8-6.8.0/        # 内核源码（从 Ubuntu HWE 源获取，不纳入 Git）
│   ├── include/linux/
│   │   └── oscdk_proc.h        # 新增：进程信息数据结构声明
│   ├── kernel/
│   │   ├── oscdk_proc.c        # 新增：三个系统调用实现（548/549/550）
│   │   └── Makefile            # 修改：添加 oscdk_proc.o
│   └── arch/x86/entry/syscalls/
│       └── syscall_64.tbl      # 修改：添加系统调用号 548-550
├── patches/                     # 补丁文件（纳入 Git）
│   └── 0001-add-proc-syscalls.patch  # 所有修改的单体补丁
├── test.c                       # 用户态测试程序（纳入 Git）
├── test.txt                     # 测试用例（可选）
└── README.md                    # 本文件
```

## 获取内核源码

本实验基于 `6.8.0-134-generic` 内核，实际源码包名为 `linux-hwe-6.8`，对应版本 `6.8.12`。使用以下命令获取匹配的源码：

```bash
cd ~/oscd/oscdk
apt source linux-image-unsigned-$(uname -r)
# 若失败，可尝试：
# apt source linux-image-$(uname -r)
```

解压后的目录名类似 `linux-hwe-6.8-6.8.0`，重命名或直接使用该目录。

## 内核修改内容

| 文件 | 操作 | 说明 |
| :--- | :--- | :--- |
| `include/linux/oscdk_proc.h` | **新增** | 定义 `struct proc_info` 和 `struct proc_stat` 以及系统调用函数声明 |
| `kernel/oscdk_proc.c` | **新增** | 实现 `sys_proc_collect`(548)、`sys_proc_snapshot`(549)、`sys_proc_stat`(550) |
| `arch/x86/entry/syscalls/syscall_64.tbl` | **修改** | 添加 548/549/550 三个系统调用表项 |
| `kernel/Makefile` | **修改** | 增加 `obj-y += oscdk_proc.o` |

## 补丁管理

所有对内核源码的修改已整合为单一补丁文件 `patches/0001-add-proc-syscalls.patch`。

### 生成补丁

首先确保已备份原始内核源码目录（如 `linux-hwe-6.8-6.8.0-orig`），然后在 `oscd/oscdk` 下执行：

```bash
diff -ruN /dev/null linux-hwe-6.8-6.8.0/include/linux/oscdk_proc.h \
  > patches/0001-add-proc-syscalls.patch
diff -ruN /dev/null linux-hwe-6.8-6.8.0/kernel/oscdk_proc.c \
  >> patches/0001-add-proc-syscalls.patch
diff -ruN linux-hwe-6.8-6.8.0-orig/arch/x86/entry/syscalls/syscall_64.tbl \
  linux-hwe-6.8-6.8.0/arch/x86/entry/syscalls/syscall_64.tbl \
  >> patches/0001-add-proc-syscalls.patch
diff -ruN linux-hwe-6.8-6.8.0-orig/kernel/Makefile \
  linux-hwe-6.8-6.8.0/kernel/Makefile \
  >> patches/0001-add-proc-syscalls.patch
```

### 应用补丁

若需从原始源码恢复修改，在源码目录内执行：

```bash
cd linux-hwe-6.8-6.8.0
patch -p1 < ../patches/0001-add-proc-syscalls.patch
```

## 编译与安装内核

1. **配置内核**  
   建议先禁用不必要的模块以加速编译（特别是 bcachefs）：
   ```bash
   cd linux-hwe-6.8-6.8.0
   cp /boot/config-$(uname -r) .config
   # 可选：设置本地版本后缀，编译后内核显示为 6.8.12-oscdk
   make olddefconfig
   # 禁用 bcachefs（可能引起编译错误）
   sed -i 's/^CONFIG_BCACHEFS_FS=m$/# CONFIG_BCACHEFS_FS is not set/' .config
   make olddefconfig
   ```

2. **编译**  
   首次或修改系统调用表后增量编译耗时较长（10-30分钟），后续仅修改 `oscdk_proc.c` 时增量编译极快。
   ```bash
   make -j$(nproc)          # 可选 make -j2 以降低功耗
   ```

3. **安装**  
   ```bash
   sudo make modules_install
   sudo make install
   sudo reboot
   ```

4. **验证**  
   重启后选择 `6.8.12-oscdk` 内核启动，运行：
   ```bash
   uname -r                # 应显示 6.8.12-oscdk
   ./test                  # 测试三个系统调用
   ```

## 用户态测试

编译测试程序 `test.c`（已包含在本目录）：
```bash
gcc -o test test.c
./test
```
预期输出包含进程列表、状态统计和进程树。

## 注意事项

- **内核源码不纳入 Git**：`linux-*` 目录需加入 `.gitignore`。
- **单补丁管理**：所有修改整合为一个补丁，便于整体应用或撤回。
- **编译时间**：修改系统调用表会导致大量文件重新编译，建议在插电状态下进行。使用 `make -j2` 可降低内存压力。
- **电量管理**：若无法插电，可在编译前限制 CPU 频率（`cpufreq-set -u 1.5GHz`）或转移到远程机器编译打包 `.deb`。
- **禁止内核自动更新**：锁定内核包以防止官方更新覆盖自定义内核。

## 作者

操作系统课程设计小组  
2026.07