# oscdvfs — Virtual File System

`oscdvfs` 是 OSCD 项目中的虚拟文件系统模块，运行在用户态，模拟 EXT2 文件系统的核心行为。它通过操作一个普通的二进制文件（`disk.img`）来模拟磁盘，实现超级块、位图、inode、目录项等数据结构的管理。

## 快速开始

### 编译

```bash
cd oscd/oscdvfs
make
```

### 运行

```bash
./oscdvfs
```

启动后显示：

```
OSCD Virtual File System (oscdvfs) v0.1
输入 'hello' 测试，输入 'exit' 退出

oscdvfs> 
```

### 内置命令（当前版本）

| 命令 | 说明 |
| :--- | :--- |
| `hello` | 测试命令，打印欢迎信息 |
| `exit`  | 退出 `oscdvfs` |

## 目录结构

```
oscd/oscdvfs/
├── Makefile
├── README.md
├── include/
│   └── vfs.h          # 公共头文件
└── src/
    ├── main.c         # 主循环、初始化
    ├── builtin.c      # 内置命令实现
    └── exec.c         # 命令分发与执行
```

## 开发路线

`oscdvfs` 将逐步实现一个完整的模拟文件系统，并最终与 `oscdsh` 联动：

- [x] 基础交互框架（主循环、命令解析）
- [x] 内置命令（`hello`, `exit`）
- [ ] 磁盘镜像管理（创建、加载 `disk.img`）
- [ ] 超级块（Superblock）设计与管理
- [ ] 块位图与 inode 位图
- [ ] inode 表管理
- [ ] 文件操作（`create`, `delete`, `open`, `close`, `read`, `write`）
- [ ] 目录操作（`dir`, `cd`）
- [ ] 路径解析（多级目录支持）
- [ ] 用户登录（`login`）
- [ ] 权限控制（rwx 保护码）
- [ ] 导出功能（`export`）
- [ ] 与 `oscdsh` 联动（`import` / `export`）

## 设计目标

- **用户态模拟**：无需内核模块，通过 `fopen`/`fread`/`fwrite` 操作 `disk.img`
- **EXT2 风格**：模仿 EXT2 的超级块、位图、inode、数据块结构
- **权限控制**：基于用户登录的 DAC（自主访问控制）
- **持久化**：所有数据保存在 `disk.img` 中，退出后数据保留
- **独立运行**：可作为独立程序运行，也可由 `oscdsh` 通过子进程调用

## 依赖

- GCC
- GNU Make
- 标准 C 库

## 作者

操作系统课程设计（OSCD）小组