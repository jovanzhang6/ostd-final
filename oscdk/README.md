# oscdk — Linux 内核实验

本目录包含实验三（系统调用）相关的内核源码与修改记录。

## 目录结构

```
oscdk/
├── linux-*/                 # 内核源码（从 Ubuntu 源获取，不纳入 Git）
│   ├── kernel/
│   │   └── sys.c            # 核心修改文件：添加系统调用函数
│   ├── include/linux/
│   │   └── syscalls.h       # 添加系统调用声明
│   └── arch/x86/entry/syscalls/
│       └── syscall_64.tbl   # 添加系统调用表项
├── patches/                 # 补丁文件（纳入 Git）
│   ├── 0001-sys-proc-collect.patch
│   ├── 0002-sys-proc-snapshot.patch
│   ├── 0003-sys-proc-stat.patch
│   └── README.md            # 补丁说明
└── README.md                # 本文件
```

## 获取内核源码

使用 Ubuntu 官方源获取与当前系统版本一致的内核源码：

```bash
cd ~/oscd/oscdk
apt source linux-image-unsigned-$(uname -r)
```

上述命令会自动下载并解压源码到 `linux-*/` 目录。

## 补丁管理

所有对内核的修改均通过补丁文件记录，存放在 `patches/` 目录下。

### 生成补丁

修改源码前，先备份原始文件：

```bash
cp linux-*/kernel/sys.c linux-*/kernel/sys.c.orig
```

修改完成后，使用 `diff` 生成补丁：

```bash
diff -u linux-*/kernel/sys.c.orig linux-*/kernel/sys.c > patches/0001-sys-proc-collect.patch
```

### 应用补丁

如果需要在新环境中复现修改，可执行：

```bash
cd linux-*/
patch -p1 < ../patches/0001-sys-proc-collect.patch
```

## 编译与安装内核

进入内核源码目录，按以下步骤编译安装新内核：

```bash
cd linux-*

# 1. 配置内核（使用当前系统配置并裁剪）
cp /boot/config-$(uname -r) .config
make localmodconfig

# 2. 编译（根据 CPU 核心数调整 -j 参数）
make -j8

# 3. 安装模块和内核
sudo make modules_install
sudo make install

# 4. 重启进入新内核
sudo reboot
```

重启后验证内核版本：

```bash
uname -r
```

## 注意事项

- **不要将内核源码纳入 Git**：源码体积大，且非本人所写，仅通过补丁记录修改。
- **补丁文件是核心产出**：所有修改必须生成补丁并提交至 `patches/` 目录。
- **每次修改后编译测试**：确保新增的系统调用功能正常。
- **保留原始文件备份**：方便生成补丁和回退。