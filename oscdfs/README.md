# oscdfs — OS Course Design File System

`oscdfs` 是 OSCD 项目中的用户态模拟文件系统模块，通过操作一个固定大小的二进制文件（`disk.img`）来模拟磁盘，实现超级块、位图、inode、目录项等完整的文件系统数据结构，并提供命令行交互接口。

## 快速开始

### 编译

```bash
cd oscd/oscdfs
make
```

### 创建磁盘映像

```bash
./oscdfs --init
```

生成 8 MiB 的 `disk.img` 并写入超级块、位图、inode 表、根目录和用户家目录。

### 运行交互模式

```bash
./oscdfs
```

启动后自动以 `root` 身份登录，进入 `/home/root`，显示提示符：

```
OSCD File System (oscdfs)
输入 'dir' 查看当前目录, 'exit' 退出

oscdfs> 
```

## 内置命令

| 命令 | 说明 | 示例 |
| :--- | :--- | :--- |
| `dir [path]` | 列出目录内容（inode、名称、物理块、权限、大小、uid） | `dir /` |
| `create <path>` | 创建空文件 | `create /home/root/test.txt` |
| `delete <path>` | 删除文件（检查占用与权限） | `delete /home/root/test.txt` |
| `open <path> [r\|w\|rw]` | 打开文件并返回文件描述符 | `open /etc/passwd r` |
| `close <fd>` | 关闭文件描述符 | `close 0` |
| `read <fd> <n>` | 从文件读取 n 字节 | `read 0 100` |
| `write <fd> <data>` | 向文件写入字符串 | `write 0 HelloWorld` |
| `cd [path]` | 切换工作目录（无参数回到用户家目录） | `cd /home/oscd` |
| `pwd` | 显示当前工作目录的绝对路径 | `pwd` |
| `login <user> <password>` | 用户登录，切换身份和家目录 | `login oscd oscd` |
| `chmod <path> <mode>` | 修改文件权限（八进制，仅所有者或 root） | `chmod /test 644` |
| `chown <path> <uid> [gid]` | 修改文件所有者/组（仅 root） | `chown /test 1000` |
| `hello` | 测试命令，打印欢迎信息 | `hello` |
| `exit` | 退出并关闭磁盘 | `exit` |

## 目录结构

```
oscd/oscdfs/
├── Makefile
├── README.md
├── test.txt               # 功能测试脚本
├── include/
│   └── oscdfs.h           # 公共头文件（数据结构、常量、函数声明）
└── src/
    ├── main.c             # 入口、参数处理、主循环
    ├── builtin.c          # 所有内置命令的实现
    ├── exec.c             # 命令分词与派发
    ├── disk.c             # 块级磁盘读写与锁保护
    ├── bitmap.c           # 块位图和 inode 位图分配/回收
    ├── super.c            # 超级块读写与空闲计数更新
    ├── inode.c            # inode 表读写、块映射与释放
    ├── dir.c              # 目录操作、路径解析、路径字符串重建
    ├── file.c             # VFS 层：文件创建/打开/关闭/读写/删除/权限/属性
    ├── mkfs.c             # 磁盘初始化（mkfs）
    ├── state.c            # 全局状态（当前目录、用户、fd 表、路径字符串）
    ├── user.c             # 用户表管理、登录、家目录查询
    └── permission.c       # 权限检查
```

## 开发路线

- [x] 基础交互框架（主循环、命令解析）
- [x] 磁盘镜像管理（创建、加载 `disk.img`）
- [x] 超级块（Superblock）设计与管理
- [x] 块位图与 inode 位图
- [x] inode 表管理
- [x] 文件操作（`create`, `delete`, `open`, `close`, `read`, `write`）
- [x] 目录操作（`dir`, `cd`, `pwd`）
- [x] 路径解析（多级目录支持，绝对/相对路径，`.` 和 `..`）
- [x] 用户登录（`login`）与家目录隔离
- [x] 权限控制（rwx 保护码，`chmod`, `chown`）
- [x] 间接块支持（单级间接块，文件最大约 4 MiB）
- [x] 文件删除保护（已打开的文件不允许删除）
- [ ] FUSE 挂载模式（与实验一联动）
- [ ] 监控/驱动数据持久化目录（与实验五/四对接）

## 设计目标

- **用户态模拟**：通过标准 C 文件 I/O 操作 `disk.img`，无需内核模块。
- **EXT2 风格**：超级块、位图、inode、数据块的布局借鉴 EXT2 思想。
- **权限控制**：基于用户/组的自主访问控制（DAC），支持 `rwx` 权限位。
- **持久化**：所有数据和元数据立即写回 `disk.img`，退出后不丢失。
- **独立运行**：可作为独立程序演示全部文件系统功能，也可被实验一的总控台调用。

## 测试

执行测试脚本前，请确保已编译并初始化磁盘：

```bash
make
./oscdfs --init
```

然后启动交互模式，按照 `test.txt` 中的命令逐条执行，对照注释检查输出。

## 依赖

- GCC
- GNU Make
- 标准 C 库
- pthread（线程互斥锁）

## 作者

操作系统课程设计小组  
2026.07