// oscdfs/src/user.c
#include "oscdfs.h"

#define USER_TABLE_BLOCK  4
#define GROUP_TABLE_BLOCK 5

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
    users[0].root_inode = 1;   /* 初始设为根，mkfs 会更新 */

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

    if (write_block(USER_TABLE_BLOCK, users) != OSCDFS_BLOCK_SIZE) {
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
    if (read_block(USER_TABLE_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
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
    if (write_block(USER_TABLE_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
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

                /* 更新当前工作目录字符串表示 */
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

/* 获取当前用户的家目录 inode */
uint32_t get_user_home_inode(void)
{
    struct oscdfs_user users[OSCDFS_MAX_USERS];
    if (read_user_table(users) != 0)
        return 1;   /* 出错返回根 */

    for (int i = 0; i < OSCDFS_MAX_USERS; i++) {
        if (users[i].uid == current_uid && users[i].username[0] != '\0') {
            return users[i].root_inode;
        }
    }
    return 1;   /* 找不到返回根 */
}