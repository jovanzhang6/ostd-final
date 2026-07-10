// oscdfs/src/permission.c
#include "oscdfs.h"

/*
 * 检查当前用户对指定 inode 是否有 access_type 所描述的权限
 * access_type: 按位或组合 OSCDFS_R_OK (4), OSCDFS_W_OK (2), OSCDFS_X_OK (1)
 * 返回值: 0 表示允许, -1 表示拒绝
 */
int oscdfs_check_permission(struct oscdfs_inode *inode, uint32_t access_type)
{
    /* root 用户拥有全部权限 */
    if (current_uid == 0)
        return 0;

    /* 提取对应角色的权限位（低 9 位中对应部分） */
    uint32_t role_bits;
    if (current_uid == inode->uid) {
        role_bits = (inode->mode >> 6) & 7;      /* owner */
    } else if (current_gid == inode->gid) {
        role_bits = (inode->mode >> 3) & 7;      /* group */
    } else {
        role_bits = inode->mode & 7;              /* other */
    }

    /* 检查是否所有请求的权限位都已设置 */
    if ((role_bits & access_type) == access_type)
        return 0;

    return -1;   /* 权限不足 */
}