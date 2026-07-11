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
│                    oscdsh (实验一：Shell)                      │
│                    统一的命令行入口                            │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │
│  │  task    │  │  driver  │  │  monitor │  │    oscdfs    │  │
│  │ (实验三) │  │ (实验四) │  │ (实验五) │  │  (实验二)    │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────┘  │
│       │              │              │               │          │
│       ▼              ▼              ▼               ▼          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │
│  │系统调用  │  │内核模块  │  │ /proc    │  │  disk.img    │  │
│  │(内核态)  │  │(内核态)  │  │(用户态)  │  │  (模拟磁盘)  │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

> 五个实验在图中分别对应：实验一（oscdsh，总控台）、实验二（oscdfs，存储层）、实验三（task，系统调用）、实验四（driver，内核代理）、实验五（monitor，观测层）。

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
│   │   └── oscdsh.h              # 主头文件
│   └── src/
│       ├── main.c                 # 主循环、信号处理、变量展开、启动横幅
│       ├── builtin.c             # 内置命令实现与统一帮助（12个命令）
│       ├── exec.c                # 命令解析、管道、重定向（含数字fd）、逻辑运算
│       ├── history.c             # 历史记录管理（添加、展开、持久化、清空）
│       ├── alias.c               # 别名存储与检索（增删查）
│       ├── completion.c          # Tab 补全（命令名/文件名，防资源泄漏）
│       ├── prompt.c              # 彩色动态提示符（时间戳、用户/主机/目录着色）
│       └── jobs.c                # 后台作业管理（SIGCHLD 异步安全通知）
├── oscdfs/                        # 实验二：模拟文件系统（原oscdvfs，现更名为oscdfs）
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
│       ├── super.c               # 超级块读写与空闲计数更新
│       ├── inode.c               # inode 表读写、块映射、间接块管理
│       ├── dir.c                 # 目录操作、路径解析、路径字符串重建
│       ├── file.c                # VFS 层（文件创建/打开/关闭/读写/删除/属性）
│       ├── mkfs.c                # 磁盘初始化（mkfs）
│       ├── state.c               # 全局状态（当前目录、用户、fd表、路径）
│       ├── user.c                # 用户表管理、登录、家目录查询、Linux UID 映射
│       ├── permission.c          # 权限检查
│       └── fuse.c                # FUSE 回调实现（所有操作映射到 VFS 层）
├── oscdk/                         # 实验三：内核系统调用
│   ├── README.md
│   ├── patches/                   # 内核修改补丁
│   │   └── README.md
│   └── linux-*/                   # 内核源码（不纳入 Git）
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
| 实验一 | `oscdsh` | ✅ **已完成** | 外部命令执行、管道、重定向（含数字文件描述符）、后台作业与 `jobs`、12个内置命令（均支持 `--help`）、逻辑运算符 `&&`/`\|\|`、彩色动态提示符（含实时时间戳）、彩色启动横幅、历史记录（持久化 + `!!`/`!n`/`!?string?` 扩展）、别名（含单引号跨参数拼接）、Tab 补全（命令名/文件名）、环境变量展开（`$VAR`/`${VAR}`/`$$`/`$?`）、SIGCHLD 异步安全回收 |
| 实验二 | `oscdfs` | ✅ **已完成** | 8 MiB 磁盘映像 `disk.img`；超级块、位图、inode 表、数据块；支持十余条内置命令（`dir`、`create`、`delete`、`open`、`close`、`read`、`write`、`cd`、`pwd`、`login`、`chmod`、`chown` 等）；多用户（root, oscd, pyc, guest）与密码登录；rwx 权限检查与用户隔离；单级间接块支持；文件删除保护；**FUSE 挂载模式**，挂载后标准 Linux 命令透明访问，支持 `allow_other` 多用户并发；彩色动态提示符（带时间戳）与 ASCII 艺术启动画面 |
| 实验三 | `oscdk` | ⚠️ 目录已准备 | 待下载内核源码、添加系统调用 |
| 实验四 | `oscddrv` | ✅ **基础部分已完成** | 字符设备注册与 `/dev/oscddrv` 自动创建；`open`/`release`/`read`/`write` 文件操作；全局 4KB 缓冲区（可动态调整）；多进程独立读写偏移；`ioctl` 控制接口（状态查询、重置、缓冲区大小调整、模式切换） |
| 实验五 | `oscdmon` | ✅ 骨架已完成 | 读取 CPU/内存/负载并输出概览 |

## 构建与运行

### 构建单个模块

```bash
# 实验一：Shell
cd oscdsh && make && ./oscdsh

# 实验二：模拟文件系统（若已存在 disk.img 可跳过 --init）
cd oscdfs && make && ./oscdfs --init && ./oscdfs

# 实验四：内核驱动（编译）
cd oscddrv && make

# 实验四：加载驱动（需 root）
sudo insmod oscddrv.ko        # 或 make install

# 实验四：运行完整测试
# 加载后参照 test.txt 逐行执行，或编译并运行 ioctl 测试程序：
cd oscddrv && gcc test.c -o test && ./test

# 实验五：系统监控
cd oscdmon && make && ./oscdmon
```

### Docker 快速启动（推荐）

项目提供了 Dockerfile，可快速搭建一致的测试环境，无需手动安装依赖。容器内已预装 `libreadline-dev` 和 `libfuse-dev`，并额外安装了 `bear` 和内核头文件（方便开发时生成代码智能提示数据库）。编译后的 `oscdsh` 和 `oscdfs` 两个命令均可直接使用。

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

> **提示**：使用 `oscdfs` 前，请先执行 `oscdfs --init` 生成 `disk.img`（通常只需一次）。内核驱动（`oscddrv`）的加载与测试需在虚拟机（或物理机）中进行，并注意内核版本匹配。

### 整体构建（待实现）

```bash
# 一键构建所有用户态模块（实验一、二、五）
make all

# 清理所有构建产物
make clean
```

> **注意**：实验三（内核源码修改）和实验四（内核驱动加载）需要在虚拟机环境中进行，且涉及内核操作，不包含在统一的用户态构建中。

## 开发路线图

| 阶段 | 任务 | 涉及实验 |
| :--- | :--- | :--- |
| **阶段一** | 下载内核源码，修改 `kernel/sys.c`，添加 3 个系统调用（548/549/550），编译并安装新内核 | 实验三 |
| **阶段二** | 在 oscdsh 中实现 `task list/snapshot/stat/export`，通过 `syscall()` 调用实验三系统调用 | 实验一 + 实验三 |
| **阶段三** | 扩展 oscdmon 监控模块：`monitor process`、`monitor memory`、`monitor network`、`monitor power`、`monitor save`、`monitor start --daemon` | 实验五 |
| **阶段四** | 实验四驱动：设备注册、`read/write/ioctl`、多进程独立偏移、`driver` 命令、`sched_test` 命令 | 实验四 |
| **阶段五** | 五实验联动演示：`monitor start` → `task list` → `sched_test` → `monitor save` → `oscdfs` 持久化查看 | 全部实验 |

## 五实验融合计划

| 实验 | 融合方式 |
| :--- | :--- |
| **实验一** | 作为总控 Shell，可直接通过子进程启动 oscdfs 交互模式，或通过 FUSE 挂载点透明操作虚拟磁盘。 |
| **实验二** | 通过 FUSE 挂载提供标准文件系统接口，监控数据、驱动测试结果等均可持久化到 `disk.img` 中；也可由 oscdsh 作为子进程调用。 |
| **实验三** | 通过自定义系统调用（548/549/550）为 `task` 命令提供内核态进程数据。 |
| **实验四** | 通过 `ioctl` 接口接收 `driver` 和 `sched_test` 命令，作为调度测试引擎。 |
| **实验五** | 通过 `monitor` 系列命令读取 `/proc`/`/sys`，并通过挂载点将监控历史写入 oscdfs 进行持久化。 |

## 开发环境要求

- **操作系统**：Ubuntu 22.04 LTS（虚拟机）
- **编译器**：GCC 12.x（编译内核模块需与内核版本匹配）
- **内核版本**：6.8.0（当前系统内核）
- **依赖包**：
  ```bash
  sudo apt install build-essential libncurses-dev bison flex libssl-dev libelf-dev dwarves fakeroot linux-headers-$(uname -r) libreadline-dev libfuse-dev
  ```

## 开发辅助：代码智能提示（IntelliSense）

项目支持通过 `compile_commands.json` 为 VS Code 提供精确的代码补全、错误检测与跳转能力，消除因内核头文件与用户态头文件混用导致的虚假报错。

### 使用 Bear 生成编译数据库

项目各子目录（`oscdsh`、`oscdfs`、`oscddrv`、`oscdmon`）均需**分别**生成 `compile_commands.json`。进入对应目录后执行：

```bash
bear -- make
```

该命令会捕获实际编译过程并生成数据库文件。**该文件包含本地绝对路径，请勿提交到 Git**（已在 `.gitignore` 中忽略）。

### 在 Docker 容器内生成

提供的 Dockerfile 已内置 `bear`，启动容器后即可直接使用：

```bash
# 以 oscddrv 为例
cd /oscd/oscddrv
bear -- make
```

其他子目录同理。生成后，VS Code 打开 `/oscd` 工作区即可自动加载各目录下的 `compile_commands.json`，无需额外配置。

### 本地开发

若未使用 Docker，需手动安装 Bear：

```bash
sudo apt install bear
```

然后按上述命令生成数据库。建议在每次新增/删除源文件、修改编译选项后重新运行 `bear -- make`，以保持 IntelliSense 准确。

### VS Code 配置

项目根目录的 `.vscode/c_cpp_properties.json` 已配置为自动搜索子目录中的 `compile_commands.json`，开发者无需手动切换配置即可同时获得内核模块与用户态程序的智能提示。该文件已提交到仓库，开箱即用。

> **提示**：如果 IDE 中仍有报错但 `make` 成功，请尝试运行 `bear -- make` 刷新数据库，或执行 `C/C++: Reset IntelliSense Database` 命令。

## 作者

操作系统课程设计小组  
2026.07