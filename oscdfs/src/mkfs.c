// oscdfs/src/mkfs.c
#include "oscdfs.h"

/* 辅助：创建目录并自动添加 '.' 和 '..' */
static int create_dir(uint32_t parent_ino, const char *name,
                      uint32_t owner_uid, uint32_t owner_gid)
{
    int dir_ino = alloc_inode();
    if (dir_ino < 0) return -1;

    inode_init((uint32_t)dir_ino, OSCDFS_DEFAULT_DIR_MODE, owner_uid, owner_gid);

    struct oscdfs_inode dir_node;
    read_inode((uint32_t)dir_ino, &dir_node);
    int blk = inode_get_block(&dir_node, 0, 1);
    if (blk < 0) {
        free_inode((uint32_t)dir_ino);
        return -1;
    }

    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        free_inode((uint32_t)dir_ino);
        return -1;
    }
    memset(buf, 0, OSCDFS_BLOCK_SIZE);
    struct oscdfs_dir_entry *entries = (struct oscdfs_dir_entry *)buf;
    entries[0].inode_no = (uint32_t)dir_ino;
    strncpy(entries[0].name, ".", 27);
    entries[1].inode_no = parent_ino;
    strncpy(entries[1].name, "..", 27);

    if (write_block((uint32_t)blk, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        free_inode((uint32_t)dir_ino);
        return -1;
    }
    free(buf);

    dir_node.size = OSCDFS_BLOCK_SIZE;
    dir_node.mtime = (uint32_t)time(NULL);
    write_inode((uint32_t)dir_ino, &dir_node);

    if (dir_add_entry(parent_ino, name, (uint32_t)dir_ino) != 0) {
        inode_free_blocks((uint32_t)dir_ino, &dir_node);
        free_inode((uint32_t)dir_ino);
        return -1;
    }
    return dir_ino;
}

int mkfs_disk(const char *path)
{
    int fd;
    struct stat st;

    if (stat(path, &st) == 0) {
        printf("oscdfs: mkfs: '%s' already exists, overwriting...\n", path);
    }

    fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("oscdfs: mkfs: open");
        return -1;
    }

    if (ftruncate(fd, OSCDFS_TOTAL_BLOCKS * OSCDFS_BLOCK_SIZE) < 0) {
        perror("oscdfs: mkfs: ftruncate");
        close(fd);
        unlink(path);
        return -1;
    }
    close(fd);

    if (disk_open(path) != 0) {
        unlink(path);
        return -1;
    }

    super_init();
    block_bitmap_init();
    inode_bitmap_init();

    /* 初始化组描述符表 */
    uint8_t *gdesc_buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!gdesc_buf) {
        fprintf(stderr, "oscdfs: mkfs: malloc failed\n");
        disk_close(); unlink(path); return -1;
    }
    memset(gdesc_buf, 0, OSCDFS_BLOCK_SIZE);
    struct oscdfs_group_desc *gdesc = (struct oscdfs_group_desc *)gdesc_buf;
    gdesc->block_bitmap_block = OSCDFS_BLOCK_BITMAP_BLOCK;
    gdesc->inode_bitmap_block = OSCDFS_INODE_BITMAP_BLOCK;
    gdesc->inode_table_block  = OSCDFS_INODE_TABLE_BLOCK;
    gdesc->free_blocks_count  = OSCDFS_TOTAL_BLOCKS - 7;  /* 元数据占块0~6共7块 */
    gdesc->free_inodes_count  = OSCDFS_TOTAL_INODES;
    if (write_block(OSCDFS_GROUP_DESC_BLOCK, gdesc_buf) != OSCDFS_BLOCK_SIZE) {
        fprintf(stderr, "oscdfs: mkfs: write group descriptor failed\n");
        free(gdesc_buf);
        disk_close(); unlink(path); return -1;
    }
    free(gdesc_buf);

    /* 根目录 */
    int root_ino = alloc_inode();
    if (root_ino != 1) {
        fprintf(stderr, "oscdfs: mkfs: unexpected root inode %d (expected 1)\n", root_ino);
        disk_close();
        unlink(path);
        return -1;
    }
    inode_init((uint32_t)root_ino, OSCDFS_DEFAULT_DIR_MODE, 0, 0);
    if (dir_init_root((uint32_t)root_ino) != 0) {
        fprintf(stderr, "oscdfs: mkfs: failed to init root directory\n");
        disk_close();
        unlink(path);
        return -1;
    }

    /* 用户表初始写入 */
    user_table_init();

    /* 创建 /home */
    int home_ino = create_dir(1, "home", 0, 0);
    if (home_ino < 0) {
        fprintf(stderr, "oscdfs: mkfs: failed to create /home\n");
        disk_close(); unlink(path); return -1;
    }

    /* 创建四个用户家目录并更新用户表 */
    struct oscdfs_user users[OSCDFS_MAX_USERS];
    read_user_table(users);

    const char *usernames[] = {"root", "oscd", "pyc", "guest"};
    uint32_t uids[] = {0, 1000, 1001, 2000};
    uint32_t gids[] = {0, 1000, 1000, 2000};

    for (int i = 0; i < 4; i++) {
        int user_ino = create_dir((uint32_t)home_ino, usernames[i], uids[i], gids[i]);
        if (user_ino < 0) {
            fprintf(stderr, "oscdfs: mkfs: failed to create /home/%s\n", usernames[i]);
            disk_close(); unlink(path); return -1;
        }

        for (int j = 0; j < OSCDFS_MAX_USERS; j++) {
            if (users[j].uid == uids[i] && users[j].username[0] != '\0') {
                users[j].root_inode = (uint32_t)user_ino;
                break;
            }
        }
    }
    write_user_table(users);

    disk_close();
    printf("oscdfs: mkfs: '%s' created successfully (8MB, %u blocks)\n",
           path, OSCDFS_TOTAL_BLOCKS);
    return 0;
}