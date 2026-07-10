// oscdfs/include/oscdfs.h
#ifndef OSCDFS_H
#define OSCDFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/file.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

/* 磁盘布局常量 */
#define OSCDFS_BLOCK_SIZE           4096
#define OSCDFS_TOTAL_BLOCKS         2048            /* 8MB / 4096 */
#define OSCDFS_TOTAL_INODES         128
#define OSCDFS_SUPERBLOCK_OFFSET    0
#define OSCDFS_BLOCK_BITMAP_OFFSET  4096
#define OSCDFS_INODE_BITMAP_OFFSET  8192
#define OSCDFS_INODE_TABLE_OFFSET   12288
#define OSCDFS_DATA_OFFSET          143360          /* 12288 + 128 * 32 */
#define OSCDFS_MAX_FILENAME         28
#define OSCDFS_DIR_ENTRY_SIZE       32              /* 4 + 28 */
#define OSCDFS_DIR_ENTRIES_PER_BLOCK (OSCDFS_BLOCK_SIZE / OSCDFS_DIR_ENTRY_SIZE)  /* 128 */
#define OSCDFS_MAX_USERS            16
#define OSCDFS_MAX_GROUPS           8
#define OSCDFS_INDIRECT_POINTERS    (OSCDFS_BLOCK_SIZE / sizeof(uint32_t)) /* 1024 */

/* 命令输入常量 */
#define MAX_CMD_LEN  1024
#define PROMPT       "oscdfs> "

/* 文件 / 目录权限与类型宏 */
/* 文件类型（高位） */
#define OSCDFS_S_IFREG   0100000   /* 普通文件 */
#define OSCDFS_S_IFDIR   0040000   /* 目录 */
/* 权限位 */
#define OSCDFS_S_IRUSR   0000400   /* 所有者读 */
#define OSCDFS_S_IWUSR   0000200   /* 所有者写 */
#define OSCDFS_S_IXUSR   0000100   /* 所有者执行 */
#define OSCDFS_S_IRGRP   0000040   /* 组读 */
#define OSCDFS_S_IWGRP   0000020   /* 组写 */
#define OSCDFS_S_IXGRP   0000010   /* 组执行 */
#define OSCDFS_S_IROTH   0000004   /* 其他读 */
#define OSCDFS_S_IWOTH   0000002   /* 其他写 */
#define OSCDFS_S_IXOTH   0000001   /* 其他执行 */

/* 常见权限组合 */
#define OSCDFS_DEFAULT_DIR_MODE   (OSCDFS_S_IFDIR | 0755)
#define OSCDFS_DEFAULT_FILE_MODE  (OSCDFS_S_IFREG | 0644)

/* 文件打开模式（用于 fd_table） */
#define OSCDFS_O_RDONLY  0
#define OSCDFS_O_WRONLY  1
#define OSCDFS_O_RDWR    2

/* 文件描述符表相关常量 */
#define OSCDFS_MAX_OPEN_FILES  64

/* 魔数 */
#define OSCDFS_MAGIC  0x4F534344   /* "OSCD" */

/* 磁盘数据结构（__attribute__((packed)) 确保无填充） */

/* 超级块 */
struct oscdfs_superblock {
    uint32_t magic;          /* 魔数 */
    uint32_t total_blocks;   /* 总块数 */
    uint32_t free_blocks;    /* 空闲块数 */
    uint32_t total_inodes;   /* 总 inode 数 */
    uint32_t free_inodes;    /* 空闲 inode 数 */
    uint32_t root_inode;     /* 根目录 inode */
    uint32_t block_size;     /* 块大小 */
    uint32_t reserved[25];   /* 保留填充至 4096 字节 */
} __attribute__((packed));

/* inode */
struct oscdfs_inode {
    uint32_t mode;           /* 文件类型及权限 */
    uint32_t uid;            /* 所有者 uid */
    uint32_t gid;            /* 所属组 gid */
    uint32_t size;           /* 文件大小（字节） */
    uint32_t ctime;          /* 创建时间 */
    uint32_t mtime;          /* 修改时间 */
    uint32_t blocks[12];     /* 直接块指针 */
    uint32_t indirect;       /* 一级间接块指针 */
} __attribute__((packed));

/* 目录项 */
struct oscdfs_dir_entry {
    uint32_t inode_no;       /* inode 号，0 表示空闲 */
    char name[28];           /* 文件名，不足 28 字节时以 '\0' 填充 */
} __attribute__((packed));

/* 用户 */
struct oscdfs_user {
    char username[32];
    char password[64];       /* 预留哈希，初期可存明文 */
    uint32_t uid;
    uint32_t gid;
    uint32_t linux_uid;      /* 映射到 Linux 真实 uid */
    uint32_t root_inode;     /* 用户根目录 inode（暂保留） */
} __attribute__((packed));

/* 组 */
struct oscdfs_group {
    char name[32];
    uint32_t gid;
    uint32_t members[16];    /* 成员 uid 列表 */
    int member_count;
} __attribute__((packed));

/* 内存中的文件描述符 */
struct oscdfs_fd {
    uint32_t inode_no;
    uint32_t offset;
    uint32_t mode;           /* OSCDFS_O_RDONLY 等 */
    int used;                /* 1 表示占用 */
};

/* 内置命令声明 */
int builtin_hello(char **args);
int builtin_exit(char **args);

int execute_command(char *line);

/* 磁盘操作函数声明 */
int  disk_open(const char *path);
void disk_close(void);
int  read_block(uint32_t block_no, void *buf);
int  write_block(uint32_t block_no, const void *buf);

/* 位图操作函数声明 */
void block_bitmap_init(void);
void inode_bitmap_init(void);
int  alloc_block(void);
int  free_block(uint32_t block_no);
int  alloc_inode(void);
int  free_inode(uint32_t inode_no);

/* 超级块操作函数声明 */
void super_init(void);
int  read_superblock(struct oscdfs_superblock *sb);
int  write_superblock(const struct oscdfs_superblock *sb);
int  super_update_counts(int delta_blocks, int delta_inodes);

/* Inode 操作函数声明 */
int  read_inode(uint32_t inode_no, struct oscdfs_inode *inode);
int  write_inode(uint32_t inode_no, const struct oscdfs_inode *inode);
void inode_init(uint32_t inode_no, uint32_t mode, uint32_t uid, uint32_t gid);
int  inode_get_block(struct oscdfs_inode *inode, uint32_t logical_block, int allocate);
void inode_free_blocks(uint32_t inode_no, struct oscdfs_inode *inode);

/* 目录操作与路径解析函数声明 */
int      dir_init_root(uint32_t root_inode);
int      dir_add_entry(uint32_t dir_inode, const char *name, uint32_t entry_inode);
int      dir_find_entry(uint32_t dir_inode, const char *name);
uint32_t find_inode_by_path(const char *path, uint32_t start_inode);

/* mkfs 函数声明 */
int mkfs_disk(const char *path);

#endif /* OSCDFS_H */