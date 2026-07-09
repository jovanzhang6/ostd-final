# oscdsh — OSCD Shell

`oscdsh` 是操作系统课程设计（OSCD）项目的核心组件 —— 一个轻量级、可扩展的 Linux Shell。它作为整个实验平台的“总指挥”，负责解析用户命令、调度各实验模块，并最终将所有功能融合为一个统一的命令行环境。

## 快速开始

### 构建

在 `oscd/shell/` 目录下执行：

```bash
make
```

### 运行

```bash
./oscdsh
```

启动后，你会看到提示符：

```
OSCD Shell (oscdsh) v0.1
输入 'hello' 测试，输入 'exit' 退出

oscdsh> 
```

### 内置命令（当前版本）

| 命令 | 说明 |
| :--- | :--- |
| `hello` | 打印欢迎信息，用于测试 Shell 是否正常 |
| `exit`  | 退出 `oscdsh` |

### 外部命令

所有系统标准命令（如 `ls`, `cat`, `grep`, `echo`）均可通过 `fork + execvp` 执行。

## 目录结构

```
oscd/shell/
├── Makefile
├── README.md
├── include/
│   └── shell.h          # 公共头文件
└── src/
    ├── main.c           # 主循环、初始化
    ├── builtin.c        # 内置命令实现（hello, exit 等）
    └── exec.c           # 命令分发与外部命令执行
```

## 开发路线

`oscdsh` 将逐步扩展，最终融合全部五个实验的功能：

- [x] 基础框架（主循环、命令解析、外部命令执行）
- [x] 内置命令（`hello`, `exit`）
- [ ] 内置命令（`cd`, `type`, `history`, `alias`, `unalias`）
- [ ] 管道 `|`
- [ ] 重定向 `>`, `<`
- [ ] 后台运行 `&`
- [ ] Tab 命令补全
- [ ] 历史快捷运行（`!n`, `!!`, `!?`）
- [ ] 进程监控命令（`task list`, `task snapshot`, `task export`）—— 联动实验三
- [ ] 设备驱动控制命令（`driver write/read/status`）—— 联动实验四
- [ ] 系统监控命令（`monitor overview/process/memory/...`）—— 联动实验五
- [ ] 电源管理命令（`power mode`）
- [ ] 调度测试命令（`sched_test`）
- [ ] 启动虚拟文件系统（`vfs` 子命令）—— 联动实验二

## 设计原则

- **模块化**：每个实验功能独立于 `src/commands/` 子目录，可单独编译。
- **统一接口**：所有内置命令遵循 `int cmd_name(char **args)` 签名。
- **渐进式开发**：从最小可用版本开始，逐步增强，确保每一步都可运行。

## 依赖

- GCC
- GNU Make
- 标准 C 库

（Tab 补全功能将引入 GNU Readline 库，届时会在 Makefile 中添加链接选项。）

## 许可证

本项目仅供课程设计学习使用，不涉及开源许可证。

## 作者

操作系统课程设计（OSCD）小组