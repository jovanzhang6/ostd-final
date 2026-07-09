# oscdmon — 系统监控模块

`oscdmon` 是 OSCD 项目中的系统监控模块，运行在用户态，通过读取 `/proc` 和 `/sys` 等标准内核接口，提供系统状态的可视化展示。它最终将作为 `oscdsh` 的内置命令（`monitor` 系列）集成到统一命令行环境中。

## 快速开始

### 编译

```bash
cd oscd/oscdmon
make
```

### 运行

```bash
./oscdmon
```

示例输出：

```
========================================
  OSCD 系统监控 (oscdmon) v0.1
========================================

CPU 使用率: 12.5%
内存: 总计 15872 MB, 空闲 4096 MB, 可用 10240 MB
负载: 0.45 (1min)  0.32 (5min)  0.28 (15min)
```

## 目录结构

```
oscdmon/
├── Makefile
├── README.md
├── include/
│   └── monitor.h          # 公共头文件
└── src/
    └── main.c             # 主程序（读取 /proc 并输出概览）
```

## 当前状态

- [x] 读取 `/proc/stat` → CPU 使用率（基于采样）
- [x] 读取 `/proc/meminfo` → 内存统计
- [x] 读取 `/proc/loadavg` → 系统负载
- [ ] 读取 `/proc/[pid]/status` → 进程列表（`monitor process`）
- [ ] 读取 `/proc/slabinfo` → SLUB 缓存（`monitor memory`）
- [ ] 读取 `/proc/net/dev` → 网络流量（`monitor network`）
- [ ] 读取 `/proc/diskstats` → 磁盘 I/O（`monitor filesystem`）
- [ ] 读取 `/proc/devices` → 设备列表（`monitor device`）
- [ ] 读取 `/sys/.../cpufreq/` → 电源频率（`monitor power`）
- [ ] 数据导出功能（`monitor save`）
- [ ] 与 `oscdsh` 集成（作为内置命令）

## 设计目标

- **纯用户态**：不需要修改内核，不需要加载内核模块
- **标准接口**：通过 `/proc` 和 `/sys` 获取数据，无需特殊权限（部分命令可能需要 `sudo`）
- **模块化**：各监控维度（CPU、内存、进程、网络、磁盘等）独立实现，便于扩展和维护
- **融合集成**：最终作为 `oscdsh` 的内置命令，与实验一、二、三、四形成完整闭环

## 监控维度对照表

| 模块 | 命令 | 数据来源 | 对应已有工具 |
| :--- | :--- | :--- | :--- |
| 系统总览 | `monitor overview` | `/proc/stat`, `/proc/meminfo`, `/proc/loadavg` | `top`, `free`, `uptime` |
| 进程列表 | `monitor process` | `/proc/[pid]/status`, `/proc/[pid]/stat` | `ps aux`, `top` |
| 内存/SLUB | `monitor memory` | `/proc/meminfo`, `/proc/slabinfo` | `free`, `slabtop` |
| 网络流量 | `monitor network` | `/proc/net/dev`, `/proc/net/tcp` | `ifconfig`, `netstat` |
| 磁盘/文件系统 | `monitor filesystem` | `/proc/diskstats`, `/proc/mounts` | `iostat`, `df -h` |
| 设备列表 | `monitor device` | `/proc/devices`, `/sys/class/*` | `lsblk`, `lspci` |
| 自定义驱动状态 | `monitor device` | `ioctl(/dev/oscddrv, CMD_GET_STATS, ...)` | **无（新增）** |
| 电源/频率 | `monitor power` | `/sys/.../cpufreq/scaling_*`, `/sys/class/powercap/` | `cpupower`, `turbostat` |

## 与其它实验的联动

| 联动 | 说明 |
| :--- | :--- |
| **实验一（Shell）** | `monitor` 和 `power` 命令将作为 `oscdsh` 的内置命令 |
| **实验二（VFS）** | `monitor save` 导出的数据可通过 `oscdvfs import` 存入虚拟磁盘 |
| **实验三（系统调用）** | `task list` 与 `monitor process` 数据互相验证（自定义 syscall vs `/proc`） |
| **实验四（驱动）** | `monitor device` 通过 `ioctl` 读取驱动状态，纳入监控面板 |

## 作者

操作系统课程设计（OSCD）小组