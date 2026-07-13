
# OSCD — 操作系统课程设计

**OSCD** 既是 **Operating System Course Design**（操作系统课程设计）的缩写，也代表 **Observability System for Control and Data**（面向控制与数据的可观测性系统）。前者是项目的起点，后者是项目成型后的定位——一个集观测、探测、验证、存储于一体的操作系统实验平台。

## 项目概述

本项目的目标是将课程设计的五个实验（Shell、文件系统、系统调用、驱动程序、系统监控）融合为一个协同工作的整体。通过统一的命令行接口（`oscdsh`），用户可以完成从进程管理、文件操作、内核扩展、设备控制到系统监控的全链路操作。

## 融合系统定位

本系统是一个 **操作系统可观测性实验平台**，其核心能力分为五个层次：

| 层次 | 能力 | 对应实验 |
| :--- | :--- | :--- |
| **交互层** | 统一的命令行入口，调度所有实验功能 | 实验一（oscdsh） |
| **存储层** | 监控历史、测试结果持久化到虚拟磁盘，支持回溯 | 实验二（oscdfs） |
| **验证层** | 自定义系统调用提供独立内核数据源，与 `/proc` 对比验证 | 实验三（oscdk） |
| **探测层** | 通过驱动主动创建内核测试任务，诊断调度器行为 | 实验四（oscddrv） |
| **观测层** | 持续采集系统状态（CPU、内存、进程、网络、电源等） | 实验五（oscdmon） |

五个实验层层递进，形成 **“观测 → 探测 → 验证 → 存储 → 控制”** 的完整闭环。

## 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                    oscdsh (实验一：Shell)                        │
│                    统一的命令行入口                               │
│           内置命令: task / oscddrv / monitor / vfs ...           │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐     │
│  │  oscdk   │  │ oscddrv  │  │  monitor │  │    oscdfs    │     │
│  │ (实验三) │   │ (实验四) │  │ (实验五) │   │  (实验二)    │     │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬───────┘     │
│       │              │              │               │           │
│       ▼              ▼              ▼               ▼           │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐     │
│  │系统调用   │  │内核模块  │   │ /proc   │   │  disk.img   │      │
│  │548/549/550│ │(内核态)  │   │(用户态)  │  │  (模拟磁盘)  │      │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────────┘
```

## 目录结构

```
oscd/
├── README.md                      # 本文件
├── .gitignore                     # Git 忽略规则
├── .dockerignore                  # Docker 构建忽略文件
├── Dockerfile                     # Docker 镜像构建文件
├── oscdsh/                        # 实验一：Shell 解释器
│   ├── README.md                  # Shell 详细说明
│   ├── Makefile
│   ├── test.txt                   # 功能测试用例（逐行粘贴测试）
│   ├── include/
│   │   ├── oscdsh.h              # 主头文件
│   │   └── monitor.h             # 系统监控模块头文件
│   └── src/
│       ├── main.c                 # 主循环、信号处理、变量展开、启动横幅
│       ├── builtin.c             # 内置命令实现与统一帮助（13个命令）
│       ├── exec.c                # 命令解析、管道、重定向（含数字fd）、逻辑运算
│       ├── history.c             # 历史记录管理（添加、展开、持久化、清空）
│       ├── alias.c               # 别名存储与检索（增删查）
│       ├── completion.c          # Tab 补全（命令名/文件名，防资源泄漏）
│       ├── prompt.c              # 彩色动态提示符（时间戳、用户/主机/目录着色）
│       ├── jobs.c                # 后台作业管理（SIGCHLD 异步安全通知）
│       └── monitor.c             # 系统监控（CPU/内存/进程/网络/磁盘/设备/电源）
├── oscdfs/                        # 实验二：模拟文件系统
│   ├── README.md                  # oscdfs 详细说明
│   ├── Makefile
│   ├── test.txt                   # 完整功能测试脚本
│   ├── include/
│   │   └── oscdfs.h              # 公共头文件（结构体、宏、函数声明）
│   └── src/
│       ├── main.c                 # 入口、参数处理、主循环、彩色提示符
│       ├── builtin.c             # 所有内置命令的实现
│       ├── exec.c                # 命令分词与派发
│       ├── disk.c                # 块级磁盘读写、文件锁、互斥锁
│       ├── bitmap.c              # 块位图与 inode 位图的分配/回收
│       ├── super.c               # 超级块读写、空闲计数更新、组描述符操作
│       ├── inode.c               # inode 表读写、块映射（支持三级间接块）、atime 更新
│       ├── dir.c                 # 目录操作、路径解析、路径字符串重建
│       ├── file.c                # VFS 层（文件创建/打开/关闭/读写/删除/属性）
│       ├── mkfs.c                # 磁盘初始化（mkfs）
│       ├── state.c               # 全局状态（当前目录、用户、fd表、路径）
│       ├── user.c                # 用户表管理、登录、家目录查询、Linux UID 映射
│       ├── permission.c          # 权限检查
│       └── fuse.c                # FUSE 回调实现（所有操作映射到 VFS 层）
├── oscdk/                         # 实验三：内核系统调用
│   ├── README.md
│   ├── test.c                     # 用户态测试程序（验证系统调用）
│   ├── test.txt                   # 测试用例
│   ├── patches/                   # 内核修改补丁（纳入 Git）
│   │   └── 0001-add-proc-syscalls.patch
│   └── linux-hwe-6.8-6.8.0/       # 内核源码（不纳入 Git）
│       ├── include/linux/oscdk_proc.h   # 进程信息数据结构
│       ├── kernel/oscdk_proc.c          # 系统调用实现
│       ├── arch/x86/entry/syscalls/syscall_64.tbl  # 系统调用表
│       └── kernel/Makefile              # 编译配置
├── oscddrv/                       # 实验四：内核驱动模块
│   ├── README.md
│   ├── Makefile
│   ├── test.txt                   # 全功能逐行验证脚本
│   ├── test.c                     # ioctl 接口测试程序
│   ├── include/
│   │   └── oscddrv.h             # 公共头文件（所有 include、宏、结构体、ioctl 命令）
│   └── src/
│       └── main.c                 # 驱动主程序（所有回调、全局变量、ioctl 实现）
└── oscdmon/                       # 实验五：系统监控
    ├── README.md
    ├── Makefile
    ├── include/
    │   └── monitor.h
    └── src/
        └── main.c
```

## 各实验模块状态

| 实验 | 模块 | 当前状态 | 核心功能 |
| :--- | :--- | :--- | :--- |
| 实验一 | `oscdsh` | ✅ **已完成** | 外部命令执行、管道、重定向（含数字文件描述符）、后台作业与 `jobs`、13个内置命令（均支持 `--help`）、逻辑运算符 `&&`/`\|\|`、彩色动态提示符（含实时时间戳）、彩色启动横幅、历史记录（持久化 + `!!`/`!n`/`!?string?` 扩展）、别名（含单引号跨参数拼接）、Tab 补全（命令名/文件名）、环境变量展开（`$VAR`/`${VAR}`/`$$`/`$?`）、SIGCHLD 异步安全回收、**系统监控命令**（`monitor` 系列：overview/process/memory/network/filesystem/device/power/save） |
| 实验二 | `oscdfs` | ✅ **已完成** | 8 MiB 磁盘映像 `disk.img`；EXT2 风格单块组布局（超级块、组描述符、块位图、inode 位图、inode 表、用户表、数据区）；128 字节 inode（含 atime、12 个直接块、单间接、双间接、三间接）；支持 `dir`、`create`、`delete`、`open`、`close`、`read`、`write`、`cd`、`pwd`、`login`、`chmod`、`chown` 等命令；多用户（root, oscd, pyc, guest）与密码登录；rwx 权限检查与用户隔离；文件删除保护（已打开文件不可删）；**FUSE 挂载模式**，挂载后标准 Linux 命令透明访问，支持 `allow_other` 多用户并发；彩色动态提示符（带时间戳）与 ASCII 艺术启动画面 |
| 实验三 | `oscdk` | ✅ **已完成** | 新增三个自定义系统调用（548/549/550）：`sys_proc_collect` 收集进程信息，`sys_proc_snapshot` 生成进程树拓扑图，`sys_proc_stat` 返回进程状态统计；内核源码修改已生成补丁 `patches/0001-add-proc-syscalls.patch`；用户态测试程序 `test.c` 验证通过 |
| 实验四 | `oscddrv` | ✅ **基础部分已完成** | 字符设备注册与 `/dev/oscddrv` 自动创建；`open`/`release`/`read`/`write` 文件操作；全局 4KB 缓冲区（可动态调整）；多进程独立读写偏移；`ioctl` 控制接口（状态查询、重置、缓冲区大小调整、模式切换） |
| 实验五 | `oscdmon` | ✅ **已集成至 oscdsh** | 所有监控功能已作为内置命令集成到 oscdsh 的 `monitor` 系列命令中 |

## 构建与运行

### 构建单个模块

```bash
# 实验一：Shell
cd oscdsh && make && ./oscdsh

# 实验二：模拟文件系统（若已存在 disk.img 可跳过 --init）
cd oscdfs && make && ./oscdfs --init && ./oscdfs

# 实验三：内核系统调用（测试）
# 前提：宿主机必须已重启进入 oscdk 自定义内核（6.8.12-oscdk），不可在 Docker 中测试
cd oscdk && gcc test.c -o test && ./test

# 实验四：内核驱动（编译）
cd oscddrv && make

# 实验四：加载驱动（需 root）
sudo insmod oscddrv.ko        # 或 make install

# 实验四：运行完整测试
# 加载后参照 test.txt 逐行执行，或编译并运行 ioctl 测试程序：
cd oscddrv && gcc test.c -o test && ./test

# 实验五：系统监控（已集成到 oscdsh）
# 直接在 oscdsh 中使用 monitor 命令即可：
#   monitor              显示系统概览（CPU/内存/负载/运行时间）
#   monitor process      显示进程列表
#   monitor process bash 按名称筛选进程
#   monitor memory       显示详细内存信息
#   monitor network      显示网络流量统计
#   monitor filesystem   显示磁盘I/O和挂载信息
#   monitor device       显示设备列表
#   monitor power        显示CPU频率信息
#   monitor save out.txt 导出监控数据到文件
```

### Docker 快速启动（推荐）

项目提供了 Dockerfile，可快速搭建一致的测试环境，无需手动安装依赖。容器内已预装 `libreadline-dev` 和 `libfuse-dev`，并额外安装了 `bear` 和内核头文件。编译后的 `oscdsh` 和 `oscdfs` 可直接在容器中运行。

```bash
# 在 oscd/ 根目录下构建镜像
docker build -t oscd .

# 运行容器（默认启动 oscdsh）
docker run -it oscd

# 直接运行 oscdfs 交互模式（覆盖默认 CMD）
docker run -it oscd oscdfs

# 初始化 oscdfs 磁盘映像并进入交互模式（若磁盘已存在可去掉 --init）
docker run -it oscd bash -c "oscdfs --init && oscdfs"
```

> **重要提示**：
> - 使用 `oscdfs` 前，请先执行 `oscdfs --init` 生成 `disk.img`（通常只需一次）。
> - **Docker 容器与宿主机共享内核**，因此 **实验三的内核系统调用验证无法在 Docker 中进行**。你必须重启物理机/虚拟机进入 `oscdk` 自定义内核后，再运行 `test` 测试程序。
> - 内核驱动（`oscddrv`）的加载与测试也需在真实机器（或虚拟机）上进行，并注意内核版本匹配。

### 整体构建（待实现）

```bash
# 一键构建所有用户态模块（实验一、二、五）
make all

# 清理所有构建产物
make clean
```

> **注意**：实验三（内核源码修改）和实验四（内核驱动加载）涉及内核操作，不包含在统一的用户态构建中。

## 五实验融合计划

| 实验 | 融合方式 |
| :--- | :--- |
| **实验一（oscdsh）** | 作为总控 Shell，通过子进程或内置命令调度各模块；`task` 命令直接调用 oscdk 系统调用。 |
| **实验二（oscdfs）** | 通过 FUSE 挂载提供标准文件系统接口，监控数据、驱动测试结果等均可持久化到 `disk.img` 中。 |
| **实验三（oscdk）** | 通过自定义系统调用（548/549/550）为 `task` 命令提供内核态进程数据；同时作为监控验证的基准数据源。 |
| **实验四（oscddrv）** | 通过 `ioctl` 接口接收 `oscddrv` 和 `sched_test` 命令，作为内核调度测试引擎。 |
| **实验五（oscdmon）** | ✅ 已完成融合：`monitor` 系列命令（overview/process/memory/network/filesystem/device/power/save）已作为内置命令集成到 oscdsh 中，通过读取 `/proc` 和 `/sys` 获取系统状态。 |

## 开发环境要求

- **操作系统**：Ubuntu 22.04 LTS（虚拟机）
- **编译器**：GCC 12.x（编译内核模块需与内核版本匹配）
- **内核版本**：6.8.0（当前系统内核）
- **依赖包**：
  ```bash
  sudo apt install build-essential libncurses-dev bison flex libssl-dev libelf-dev dwarves fakeroot linux-headers-$(uname -r) libreadline-dev libfuse-dev
  ```

## 开发辅助：代码智能提示（IntelliSense）

项目支持通过 `compile_commands.json` 为 VS Code 提供精确的代码补全、错误检测与跳转能力。

### 使用 Bear 生成编译数据库

项目各子目录（`oscdsh`、`oscdfs`、`oscddrv`、`oscdmon`）均需**分别**生成 `compile_commands.json`。进入对应目录后执行：

```bash
bear -- make
```

该命令会捕获实际编译过程并生成数据库文件。**该文件包含本地绝对路径，请勿提交到 Git**（已在 `.gitignore` 中忽略）。

### 在 Docker 容器内生成

提供的 Dockerfile 已内置 `bear`，启动容器后即可直接使用：

```bash
cd /oscd/oscddrv   # 或其他子目录
bear -- make
```

VS Code 打开 `/oscd` 工作区即可自动加载各目录下的 `compile_commands.json`。

### 本地开发

若未使用 Docker，需手动安装 Bear：

```bash
sudo apt install bear
```

然后按上述命令生成数据库。建议在每次新增/删除源文件、修改编译选项后重新运行 `bear -- make`，以保持 IntelliSense 准确。

> **提示**：如果 IDE 中仍有报错但 `make` 成功，请尝试运行 `bear -- make` 刷新数据库，或执行 `C/C++: Reset IntelliSense Database` 命令。

## 作者

操作系统课程设计小组  
2026.07