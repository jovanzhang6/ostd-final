// oscdfs/src/state.c
#include "oscdfs.h"

uint32_t current_dir_inode = 1;   /* 根目录 inode 改为 1 */
uint32_t current_uid = 0;
uint32_t current_gid = 0;
struct oscdfs_fd fd_table[OSCDFS_MAX_OPEN_FILES];
char cwd_path[MAX_CMD_LEN] = "/home/root";