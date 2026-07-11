# oscdfs — OS Course Design File System

`oscdfs` 是 OSCD 项目中的用户态模拟文件系统模块。它通过一个固定大小的二进制文件（`disk.img`）模拟磁盘，实现了超级块、组描述符、位图、inode、目录项等完整的 EXT2 风格文件系统数据结构，并提供命令行交互接口与 FUSE 挂载能力。

## 快速开始

### 编译

```bash
cd oscd/oscdfs
make
```

### 创建磁盘映像

```bash
./oscdfs --init                 # 在当前目录生成 8 MiB 的 disk.img
./oscdfs --init -f mydisk.img   # 指定路径
```

### 交互模式

```bash
./oscdfs
```

启动后自动以 `root` 身份登录，进入 `/home/root`，显示彩色提示符：

```
   ██████╗ ███████╗ ██████╗██████╗ ███████╗███████╗
  ██╔═══██╗██╔════╝██╔════╝██╔══██╗██╔════╝██╔════╝
  ██║   ██║███████╗██║     ██║  ██║█████╗  ███████╗
  ██║   ██║╚════██║██║     ██║  ██║██╔══╝  ╚════██║
  ╚██████╔╝███████║╚██████╗██████╔╝██║     ███████║
   ╚═════╝ ╚══════╝ ╚═════╝╚═════╝ ╚═╝     ╚══════╝
     OS Course Design File System  v2.0

  磁盘文件: disk.img

[14:30:05] root@oscdfs /home/root $ 
```

### FUSE 挂载模式

```bash
sudo mkdir -p /mnt/oscd && sudo chmod 777 /mnt/oscd   # 一次性准备挂载点
./oscdfs --init                                        # 若已有 disk.img 可跳过
sudo ./oscdfs -m /mnt/oscd -o allow_other -s &         # 挂载后其他用户也可访问
```

现在你可以用标准 Linux 命令操作虚拟文件系统：

```bash
ls /mnt/oscd
cat /mnt/oscd/home/oscd/test.txt
echo "hello" > /mnt/oscd/home/oscd/new.txt
```

卸载并清理：
```bash
fusermount -u /mnt/oscd
```

## 文件系统结构

磁盘映像 `disk.img` 大小为 8 MiB，采用简化的 EXT2 单块组布局，包含以下区域：

| 区域 | 块号 | 说明 |
| :--- | :--- | :--- |
| 超级块 | 0 | 魔数、总块数/空闲块数/inode数、块大小等 |
| 组描述符 | 1 | 本组内块位图、inode位图、inode表位置及空闲计数 |
| 块位图 | 2 | 管理数据块的分配 |
| inode 位图 | 3 | 管理 inode 的分配 |
| inode 表 | 4～7 | 128 个 inode，每个 128 字节，包含 atime、12个直接块、一级间接块、二级间接块、三级间接块 |
| 用户表 | 8 | 用户账户信息（用户名、密码、uid/gid、家目录等） |
| 组表 | 9 | 预留的组信息 |
| 数据区 | 10～2047 | 存储文件内容和目录项 |

## 内置命令

### 交互模式命令

| 命令 | 说明 | 示例 |
| :--- | :--- | :--- |
| `dir [path]` | 列出目录内容（inode, 名称, 物理块, 权限, 大小, uid） | `dir /` |
| `create <path>` | 创建空文件 | `create /home/root/test.txt` |
| `delete <path>` | 删除文件（检查占用与权限） | `delete /home/root/test.txt` |
| `open <path> [r\|w\|rw]` | 打开文件并返回文件描述符 | `open /home/root/test.txt rw` |
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

### FUSE 模式下的透明命令

挂载后，标准 Linux 命令均可直接使用：`ls`, `cat`, `echo`, `touch`, `rm`, `mkdir`, `rmdir`, `chmod`, `chown`, `stat`, `dd` 等。它们会被自动转换成文件系统的底层操作，无需额外实现。

## 用户与权限

| 模拟用户 | 密码 | uid | 家目录 | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| `root` | `root` | 0 | `/home/root` | 超级用户，拥有所有权限 |
| `oscd` | `oscd` | 1000 | `/home/oscd` | 普通用户 |
| `pyc` | `pyc` | 1001 | `/home/pyc` | 与 oscd 同组（gid=1000） |
| `guest` | `guest` | 2000 | `/home/guest` | 受限用户 |

- **权限模型**：所有者 / 所属组 / 其他人，支持 rwx 位。
- **FUSE 用户映射**：根据 Linux 真实 UID 自动映射到模拟用户，未映射的 UID 将被视为 `guest`。

## 目录结构

```
oscd/oscdfs/
├── Makefile
├── README.md
├── test.txt               # 完整功能测试脚本
├── include/
│   └── oscdfs.h           # 公共头文件（所有结构体、宏、函数声明）
└── src/
    ├── main.c             # 入口、参数处理、主循环、彩色提示符
    ├── builtin.c          # 所有内置命令的实现
    ├── exec.c             # 命令分词与派发
    ├── disk.c             # 块级磁盘读写、文件锁、互斥锁
    ├── bitmap.c           # 块位图与 inode 位图的分配/回收
    ├── super.c            # 超级块读写、空闲计数更新、组描述符操作
    ├── inode.c            # inode 表读写、块映射（支持三级间接块）、atime 更新
    ├── dir.c              # 目录操作、路径解析、路径字符串重建
    ├── file.c             # VFS 层：文件创建/打开/关闭/读写/删除/属性
    ├── mkfs.c             # 磁盘初始化（mkfs）
    ├── state.c            # 全局状态（当前目录、用户、fd 表、路径）
    ├── user.c             # 用户表管理、登录、Linux UID 映射
    ├── permission.c       # 权限检查
    └── fuse.c             # FUSE 回调实现（所有操作映射到 VFS 层）
```

## 开发路线

- [x] 基础交互框架（主循环、命令解析）
- [x] 磁盘镜像管理（创建、加载 `disk.img`）
- [x] 超级块（Superblock）设计与管理
- [x] 组描述符表（Group Descriptor）
- [x] 块位图与 inode 位图
- [x] inode 表管理（128 字节 inode，含 atime）
- [x] 文件操作（`create`, `delete`, `open`, `close`, `read`, `write`）
- [x] 目录操作（`dir`, `cd`, `pwd`）
- [x] 路径解析（多级目录，绝对/相对，`.` 和 `..`）
- [x] 用户登录（`login`）与家目录隔离
- [x] 权限控制（rwx 保护码，`chmod`, `chown`）
- [x] 多级间接块支持（12 直接 + 单间接 + 双间接 + 三间接，文件大小受磁盘容量限制）
- [x] 文件删除保护（已打开的文件不允许删除）
- [x] FUSE 挂载模式（支持标准 Linux 命令，多用户隔离）
- [x] 彩色提示符与启动画面

## 测试

1. **交互模式测试**  
   ```bash
   make && ./oscdfs --init
   ./oscdfs
   ```
   按照 `test.txt` 中的脚本逐条执行，检查输出是否符合预期。测试覆盖了文件生命周期、权限控制、chmod/chown、多用户隔离、组描述符与 inode 空闲计数验证、多级间接块的大文件读写以及 atime 更新。

2. **FUSE 挂载测试**  
   ```bash
   sudo mkdir -p /mnt/oscd && sudo chmod 777 /mnt/oscd
   sudo ./oscdfs -m /mnt/oscd -o allow_other -s &
   # 执行标准文件操作验证，包括大文件间接块测试、atime 测试、多用户权限测试
   fusermount -u /mnt/oscd
   ```

## 依赖

- GCC
- GNU Make
- 标准 C 库
- pthread（线程互斥锁）
- libfuse-dev（FUSE 开发库）

## 作者

操作系统课程设计小组  
2026.07