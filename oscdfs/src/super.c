// oscdfs/src/super.c
#include "oscdfs.h"

/* 初始化超级块：使用完整的块缓冲区写入，根 inode 设为 1 */
void super_init(void)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: super_init: malloc failed\n");
        exit(EXIT_FAILURE);
    }
    memset(buf, 0, OSCDFS_BLOCK_SIZE);

    struct oscdfs_superblock *sb = (struct oscdfs_superblock *)buf;
    sb->magic        = OSCDFS_MAGIC;
    sb->total_blocks = OSCDFS_TOTAL_BLOCKS;
    sb->total_inodes = OSCDFS_TOTAL_INODES;
    sb->root_inode   = 1;               /* 根目录 inode 改为 1 */
    sb->block_size   = OSCDFS_BLOCK_SIZE;

    uint32_t meta_blocks = 4;           /* 超级块、两个位图、inode 表 */
    sb->free_blocks = OSCDFS_TOTAL_BLOCKS - meta_blocks;
    sb->free_inodes = OSCDFS_TOTAL_INODES;

    if (write_block(0, buf) != OSCDFS_BLOCK_SIZE) {
        fprintf(stderr, "oscdfs: super_init: write failed\n");
        free(buf);
        exit(EXIT_FAILURE);
    }
    free(buf);
}

/* 从磁盘读取超级块 */
int read_superblock(struct oscdfs_superblock *sb)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: read_superblock: malloc failed\n");
        return -1;
    }

    if (read_block(0, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    memcpy(sb, buf, sizeof(struct oscdfs_superblock));
    free(buf);

    if (sb->magic != OSCDFS_MAGIC) {
        fprintf(stderr, "oscdfs: read_superblock: bad magic 0x%08X\n", sb->magic);
        return -1;
    }
    return 0;
}

/* 将超级块写回磁盘：先读整块，修改超级块部分，再写回 */
int write_superblock(const struct oscdfs_superblock *sb)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: write_superblock: malloc failed\n");
        return -1;
    }

    if (read_block(0, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    memcpy(buf, sb, sizeof(struct oscdfs_superblock));

    if (write_block(0, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    free(buf);
    return 0;
}

/* 原子更新空闲块和空闲 inode 计数 */
int super_update_counts(int delta_blocks, int delta_inodes)
{
    struct oscdfs_superblock sb;

    if (read_superblock(&sb) != 0)
        return -1;

    if ((int)sb.free_blocks + delta_blocks < 0 ||
        (int)sb.free_blocks + delta_blocks > (int)sb.total_blocks) {
        fprintf(stderr, "oscdfs: super_update_counts: free_blocks overflow\n");
        return -1;
    }
    if ((int)sb.free_inodes + delta_inodes < 0 ||
        (int)sb.free_inodes + delta_inodes > (int)sb.total_inodes) {
        fprintf(stderr, "oscdfs: super_update_counts: free_inodes overflow\n");
        return -1;
    }

    sb.free_blocks += delta_blocks;
    sb.free_inodes += delta_inodes;

    return write_superblock(&sb);
}