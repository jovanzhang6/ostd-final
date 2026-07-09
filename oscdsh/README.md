# oshell - 操作系统课程设计 Shell

## 简介

oshell 是一个使用 C 语言开发的用户态命令解释程序，作为《操作系统课程设计》实验一的成果。  
它实现了基础 Shell 的核心功能，包括命令执行、管道、重定向、后台作业管理、内置命令、历史记录、别名替换、环境变量展开等，并预留了与其他实验模块联动的扩展接口。

## 功能特性

### 基础命令与语法
- **外部命令执行**：`fork()` + `execvp()`，支持任意系统程序（如 `ls`, `cat`, `grep`, `echo`）
- **管道** (`|`)：多级管道，`pipe()` + `dup2()` 连接进程间数据流
- **重定向**：
  - 输出重定向 (`>` / `>>`)
  - 输入重定向 (`<`)
  - 支持管道与重定向的任意组合
- **后台运行** (`&`)：父进程不阻塞，SIGCHLD 异步回收子进程，`jobs` 命令查看后台作业
- **逻辑运算符** (`&&` / `||`)：短路求值，基于命令退出状态
- **Tab 命令补全**：基于 GNU Readline，支持命令名和文件名补全

### 内置命令
| 命令 | 功能 |
|:---|:---|
| `cd [目录]` | 切换工作目录，支持 `cd -` 返回上一目录 |
| `type <命令>` | 判断命令类型（内置/外部）并显示路径 |
| `history [-c]` | 显示或清空历史记录 |
| `alias [名称=值]` | 定义或查看别名 |
| `unalias <名称>` | 删除别名 |
| `pwd` | 打印当前工作目录 |
| `export VAR=value` | 设置环境变量 |
| `jobs` | 列出后台作业及其状态 |
| `true` / `false` | 返回成功/失败，用于逻辑测试 |
| `hello` / `exit` | 测试与退出 |

### 交互增强
- **动态提示符**：格式 `[用户@主机 目录]$ `，家目录显示为 `~`，root 用户自动显示 `#`
- **历史快捷操作**：`!!`（上一条）、`!n`（第 n 条）、`!?string?`（反向搜索）
- **历史持久化**：退出时保存至 `~/.oscdsh_history`，启动时自动加载
- **环境变量展开**：`$VAR`、`${VAR}`、`$$`（PID）、`$?`（上一条命令退出码）
- **统一帮助**：所有内置命令支持 `--help` 选项

### 预留融合接口
- `task list/snapshot/stat/export` — 进程监控（实验三）
- `driver write/read/status/reset/buffer/mode` — 虚拟驱动操作（实验四）
- `monitor overview/process/memory/...` — 系统监控（实验五）
- `power mode/show` — 电源策略管理（实验五）
- `sched_test create/start/results/reset` — 调度测试（实验四）
- `vfs` — 虚拟文件系统子 Shell（实验二）

以上命令在后续实验模块完成后，以外部可执行程序或系统调用形式集成。

## 系统要求

- Linux 操作系统（推荐 Ubuntu 20.04+）
- GCC 编译器
- GNU Make
- GNU Readline 库（`libreadline-dev`）

安装依赖（Debian/Ubuntu）：
```bash
sudo apt install build-essential libreadline-dev
```

## 构建与安装

```bash
cd oscd/oscdsh
make
```

生成的可执行文件 `oscdsh` 位于当前目录。  
清理编译产物：`make clean`

## 使用方法

启动 Shell：
```bash
./oscdsh
```

进入后即可输入命令，例如：
```bash
ls -l | grep .c
echo hello > test.txt
cat < test.txt
sleep 10 &
jobs
alias ll='ls -l'
ll
cd /tmp && pwd || echo failed
history
exit
```

Tab 键可补全命令和文件名。

## 测试

项目提供了 `test.txt` 测试脚本，覆盖所有已实现功能。  
可直接在 `oscdsh` 中按顺序输入测试命令（或使用输入重定向 `./oscdsh < test.txt`）。

## 项目结构

```
oscd/oscdsh/
├── include/          # 头文件
│   └── oscdsh.h      # 主头文件
├── src/              # 源文件
│   ├── main.c        # 主循环，信号处理，变量展开
│   ├── exec.c        # 命令解析、管道、重定向、逻辑运算、外部命令执行
│   ├── builtin.c     # 内置命令实现与帮助
│   ├── history.c     # 历史记录管理（添加、展开、持久化）
│   ├── alias.c       # 别名存储与检索
│   ├── completion.c  # Tab 补全（基于 Readline）
│   ├── prompt.c      # 动态提示符生成
│   └── jobs.c        # 后台作业表与 SIGCHLD 处理
├── obj/              # 编译中间文件（自动生成）
├── Makefile
├── README.md
└── test.txt          # 功能测试用例
```

## 已知局限

- 未实现引号解析（含空格的参数需通过外部脚本或转义处理）。
- 未实现作业控制（`fg`/`bg`）和子 Shell `(cmd)`。
- 管道中禁止内置命令，但逻辑运算符内的管道同样生效该限制。
- 历史记录上限为 1024 条，作业上限为 64 个。

## 实验一完成情况

✅ P0 核心地基（主循环、外部命令、`cd`、管道、重定向、组合重定向）  
✅ P1 交互体验（后台、`type`、`history`/`!`、`alias`/`unalias`、Tab 补全）  
✅ P2 增强亮点（动态提示符、`$` 变量展开、`pwd` 内置、`&&`/`||`、SIGCHLD 回收、`true`/`false`、`--help`）  

实验一独立功能已完整交付，待实验二~五模块就绪后可无缝集成融合命令。

## 作者

操作系统课程设计小组  
2026.07