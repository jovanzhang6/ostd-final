// oscdfs/include/oscdfs.h
#ifndef OSCDFS_H
#define OSCDFS_H

/* 必须先定义特性宏，确保所有接口可见 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <fuse.h>

/* 磁盘布局常量 */
#define OSCDFS_BLOCK_SIZE           4096
#define OSCDFS_TOTAL_BLOCKS         2048
#define OSCDFS_TOTAL_INODES         128

/* 偏移量（字节） */
#define OSCDFS_SUPERBLOCK_OFFSET    0           // 块0
#define OSCDFS_GROUP_DESC_OFFSET    4096        // 块1
#define OSCDFS_BLOCK_BITMAP_OFFSET  8192        // 块2
#define OSCDFS_INODE_BITMAP_OFFSET  12288       // 块3
#define OSCDFS_INODE_TABLE_OFFSET   16384       // 块4

/* 逻辑块号 */
#define OSCDFS_GROUP_DESC_BLOCK     1
#define OSCDFS_BLOCK_BITMAP_BLOCK   2
#define OSCDFS_INODE_BITMAP_BLOCK   3
#define OSCDFS_INODE_TABLE_BLOCK    4
#define OSCDFS_USER_TABLE_BLOCK     5
#define OSCDFS_GROUP_TABLE_BLOCK    6

/* 数据区起始偏移（块7开始） */
#define OSCDFS_DATA_OFFSET          28672       // 7 * 4096

#define OSCDFS_MAX_FILENAME         28
#define OSCDFS_DIR_ENTRY_SIZE       32
#define OSCDFS_DIR_ENTRIES_PER_BLOCK (OSCDFS_BLOCK_SIZE / OSCDFS_DIR_ENTRY_SIZE)
#define OSCDFS_MAX_USERS            16
#define OSCDFS_MAX_GROUPS           8
#define OSCDFS_INDIRECT_POINTERS    (OSCDFS_BLOCK_SIZE / sizeof(uint32_t))

#define MAX_CMD_LEN  1024

/* 权限与类型 */
#define OSCDFS_S_IFREG   0100000
#define OSCDFS_S_IFDIR   0040000
#define OSCDFS_S_IRUSR   0000400
#define OSCDFS_S_IWUSR   0000200
#define OSCDFS_S_IXUSR   0000100
#define OSCDFS_S_IRGRP   0000040
#define OSCDFS_S_IWGRP   0000020
#define OSCDFS_S_IXGRP   0000010
#define OSCDFS_S_IROTH   0000004
#define OSCDFS_S_IWOTH   0000002
#define OSCDFS_S_IXOTH   0000001
#define OSCDFS_DEFAULT_DIR_MODE   (OSCDFS_S_IFDIR | 0755)
#define OSCDFS_DEFAULT_FILE_MODE  (OSCDFS_S_IFREG | 0644)

#define OSCDFS_O_RDONLY  0
#define OSCDFS_O_WRONLY  1
#define OSCDFS_O_RDWR    2

#define OSCDFS_MAX_OPEN_FILES  64
#define OSCDFS_MAGIC  0x4F534344

/* 访问权限检查用的位掩码 */
#define OSCDFS_R_OK   4
#define OSCDFS_W_OK   2
#define OSCDFS_X_OK   1

/* 数据结构 */
struct oscdfs_superblock {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t total_inodes;
    uint32_t free_inodes;
    uint32_t root_inode;
    uint32_t block_size;
    uint32_t reserved[25];
} __attribute__((packed));

/* 组描述符（新增） */
struct oscdfs_group_desc {
    uint32_t block_bitmap_block;   // 块位图所在块号
    uint32_t inode_bitmap_block;   // inode位图所在块号
    uint32_t inode_table_block;    // inode表起始块号
    uint32_t free_blocks_count;    // 本组空闲块数
    uint32_t free_inodes_count;    // 本组空闲inode数
    uint32_t reserved[10];         // 填充，保持结构体对齐
} __attribute__((packed));

struct oscdfs_inode {
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t blocks[12];
    uint32_t indirect;
} __attribute__((packed));

struct oscdfs_dir_entry {
    uint32_t inode_no;
    char name[28];
} __attribute__((packed));

struct oscdfs_user {
    char username[32];
    char password[64];
    uint32_t uid;
    uint32_t gid;
    uint32_t linux_uid;
    uint32_t root_inode;
} __attribute__((packed));

/* 用户组（与权限无关，此处保留） */
struct oscdfs_group {
    char name[32];
    uint32_t gid;
    uint32_t members[16];
    int member_count;
} __attribute__((packed));

struct oscdfs_fd {
    uint32_t inode_no;
    uint32_t offset;
    uint32_t mode;
    int used;
};

/* 全局状态 extern */
extern uint32_t current_dir_inode;
extern uint32_t current_uid;
extern uint32_t current_gid;
extern struct oscdfs_fd fd_table[OSCDFS_MAX_OPEN_FILES];
extern char cwd_path[MAX_CMD_LEN];

/* 函数声明 */
int builtin_hello(char **args);
int builtin_exit(char **args);
int execute_command(char *line);

/* 磁盘操作 */
int  disk_open(const char *path);
void disk_close(void);
int  read_block(uint32_t block_no, void *buf);
int  write_block(uint32_t block_no, const void *buf);

/* 位图 */
void block_bitmap_init(void);
void inode_bitmap_init(void);
int  alloc_block(void);
int  free_block(uint32_t block_no);
int  alloc_inode(void);
int  free_inode(uint32_t inode_no);

/* 超级块 */
void super_init(void);
int  read_superblock(struct oscdfs_superblock *sb);
int  write_superblock(const struct oscdfs_superblock *sb);
int  super_update_counts(int delta_blocks, int delta_inodes);

/* 组描述符（新增） */
int  read_group_desc(struct oscdfs_group_desc *desc);
int  write_group_desc(const struct oscdfs_group_desc *desc);
int  group_desc_update_counts(int delta_blocks, int delta_inodes);

/* Inode */
int  read_inode(uint32_t inode_no, struct oscdfs_inode *inode);
int  write_inode(uint32_t inode_no, const struct oscdfs_inode *inode);
void inode_init(uint32_t inode_no, uint32_t mode, uint32_t uid, uint32_t gid);
int  inode_get_block(struct oscdfs_inode *inode, uint32_t logical_block, int allocate);
void inode_free_blocks(uint32_t inode_no, struct oscdfs_inode *inode);

/* 目录 */
int      dir_init_root(uint32_t root_inode);
int      dir_add_entry(uint32_t dir_inode, const char *name, uint32_t entry_inode);
int      dir_find_entry(uint32_t dir_inode, const char *name);
int      dir_remove_entry(uint32_t dir_inode, const char *name);
uint32_t find_inode_by_path(const char *path, uint32_t start_inode);
void build_path_from_inode(uint32_t ino, char *buf, size_t bufsize);

/* mkfs */
int mkfs_disk(const char *path);

/* VFS 操作 */
int oscdfs_create(const char *path);
int oscdfs_open(const char *path, int flags);
int oscdfs_close(int fd);
int oscdfs_read(int fd, void *buf, uint32_t nbytes);
int oscdfs_write(int fd, const void *buf, uint32_t nbytes);
int oscdfs_delete(const char *path);
int oscdfs_chmod(const char *path, uint32_t new_mode);
int oscdfs_chown(const char *path, uint32_t new_uid, uint32_t new_gid);

/* 底层 inode 读写（供 FUSE 使用） */
int oscdfs_read_inode(uint32_t ino, void *buf, off_t offset, size_t nbytes);
int oscdfs_write_inode(uint32_t ino, const void *buf, off_t offset, size_t nbytes);

/* 用户管理 */
void user_table_init(void);
int  read_user_table(struct oscdfs_user *users);
int  write_user_table(const struct oscdfs_user *users);
int  oscdfs_login(const char *username, const char *password);
uint32_t get_user_home_inode(void);
int  oscdfs_map_linux_uid_to_sim_uid(uid_t linux_uid);
const char *get_current_username(void);
void oscdfs_set_user_from_fuse_context(void);

/* 权限检查 */
int oscdfs_check_permission(struct oscdfs_inode *inode, uint32_t access_type);

/* 命令函数 */
int builtin_dir(char **args);
int builtin_cd(char **args);
int builtin_create(char **args);
int builtin_open(char **args);
int builtin_close(char **args);
int builtin_read(char **args);
int builtin_write(char **args);
int builtin_delete(char **args);
int builtin_login(char **args);
int builtin_chmod(char **args);
int builtin_chown(char **args);
int builtin_pwd(char **args);

/* FUSE 入口 */
int oscdfs_fuse_main(int argc, char *argv[]);

#endif /* OSCDFS_H */