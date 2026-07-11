// oscdfs/src/user.c
#include "oscdfs.h"

/* 用户表块号使用 oscdfs.h 中定义的公共宏 OSCDFS_USER_TABLE_BLOCK (8) */
/* 组表块号同理 OSCDFS_GROUP_TABLE_BLOCK (9)，但当前未使用组表功能 */

void user_table_init(void)
{
    struct oscdfs_user users[OSCDFS_MAX_USERS];
    memset(users, 0, sizeof(users));

    /* root */
    strcpy(users[0].username, "root");
    strcpy(users[0].password, "root");
    users[0].uid = 0;
    users[0].gid = 0;
    users[0].linux_uid = 0;
    users[0].root_inode = 1;

    /* oscd */
    strcpy(users[1].username, "oscd");
    strcpy(users[1].password, "oscd");
    users[1].uid = 1000;
    users[1].gid = 1000;
    users[1].linux_uid = 1000;
    users[1].root_inode = 1;

    /* pyc */
    strcpy(users[2].username, "pyc");
    strcpy(users[2].password, "pyc");
    users[2].uid = 1001;
    users[2].gid = 1000;
    users[2].linux_uid = 1001;
    users[2].root_inode = 1;

    /* guest */
    strcpy(users[3].username, "guest");
    strcpy(users[3].password, "guest");
    users[3].uid = 2000;
    users[3].gid = 2000;
    users[3].linux_uid = 2000;
    users[3].root_inode = 1;

    if (write_block(OSCDFS_USER_TABLE_BLOCK, users) != OSCDFS_BLOCK_SIZE) {
        fprintf(stderr, "oscdfs: user_table_init: write failed\n");
        exit(EXIT_FAILURE);
    }
}

int read_user_table(struct oscdfs_user *users)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: read_user_table: malloc failed\n");
        return -1;
    }
    if (read_block(OSCDFS_USER_TABLE_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }
    memcpy(users, buf, sizeof(struct oscdfs_user) * OSCDFS_MAX_USERS);
    free(buf);
    return 0;
}

int write_user_table(const struct oscdfs_user *users)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: write_user_table: malloc failed\n");
        return -1;
    }
    memset(buf, 0, OSCDFS_BLOCK_SIZE);
    memcpy(buf, users, sizeof(struct oscdfs_user) * OSCDFS_MAX_USERS);
    if (write_block(OSCDFS_USER_TABLE_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }
    free(buf);
    return 0;
}

int oscdfs_login(const char *username, const char *password)
{
    struct oscdfs_user users[OSCDFS_MAX_USERS];
    if (read_user_table(users) != 0) {
        fprintf(stderr, "oscdfs: oscdfs_login: read user table failed\n");
        return -1;
    }

    for (int i = 0; i < OSCDFS_MAX_USERS; i++) {
        if (users[i].username[0] == '\0')
            continue;

        if (strcmp(users[i].username, username) == 0) {
            if (strcmp(users[i].password, password) == 0) {
                current_uid = users[i].uid;
                current_gid = users[i].gid;
                current_dir_inode = users[i].root_inode;
                build_path_from_inode(current_dir_inode, cwd_path, MAX_CMD_LEN);
                return 0;
            } else {
                fprintf(stderr, "oscdfs: oscdfs_login: wrong password for '%s'\n", username);
                return -1;
            }
        }
    }

    fprintf(stderr, "oscdfs: oscdfs_login: user '%s' not found\n", username);
    return -1;
}

uint32_t get_user_home_inode(void)
{
    struct oscdfs_user users[OSCDFS_MAX_USERS];
    if (read_user_table(users) != 0)
        return 1;

    for (int i = 0; i < OSCDFS_MAX_USERS; i++) {
        if (users[i].uid == current_uid && users[i].username[0] != '\0')
            return users[i].root_inode;
    }
    return 1;
}

int oscdfs_map_linux_uid_to_sim_uid(uid_t linux_uid)
{
    struct oscdfs_user users[OSCDFS_MAX_USERS];
    if (read_user_table(users) != 0) {
        current_uid = 0;
        current_gid = 0;
        return 0;
    }

    for (int i = 0; i < OSCDFS_MAX_USERS; i++) {
        if (users[i].username[0] != '\0' && users[i].linux_uid == (uint32_t)linux_uid) {
            current_uid = users[i].uid;
            current_gid = users[i].gid;
            return 0;
        }
    }

    current_uid = 2000;
    current_gid = 2000;
    return 0;
}

/* 从 FUSE 上下文中获取 Linux uid 并映射到模拟用户 */
void oscdfs_set_user_from_fuse_context(void)
{
    struct fuse_context *ctx = fuse_get_context();
    if (ctx) {
        oscdfs_map_linux_uid_to_sim_uid(ctx->uid);
    } else {
        current_uid = 0;
        current_gid = 0;
    }
}

/* 获取当前用户名的字符串表示（静态缓冲区，不可重入） */
const char *get_current_username(void)
{
    static char name[32] = "root";   /* 默认 root */
    struct oscdfs_user users[OSCDFS_MAX_USERS];
    if (read_user_table(users) != 0)
        return name;

    for (int i = 0; i < OSCDFS_MAX_USERS; i++) {
        if (users[i].uid == current_uid && users[i].username[0] != '\0') {
            strncpy(name, users[i].username, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            return name;
        }
    }
    return "unknown";
}