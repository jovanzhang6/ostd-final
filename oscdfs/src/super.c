// oscdfs/src/super.c
#include "oscdfs.h"

/* 超级块操作 */

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
    sb->root_inode   = 1;               /* 根目录 inode */
    sb->block_size   = OSCDFS_BLOCK_SIZE;

    /*
     * 元数据占用块数：块0(超级块), 1(组描述符), 2(块位图), 3(inode位图),
     * 4~7(inode表，共4块), 8(用户表), 9(组表)
     */
    uint32_t meta_blocks = 10;
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

/* 原子更新超级块中的空闲块和空闲 inode 计数 */
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

/* 组描述符操作 */

/* 从磁盘读取组描述符 */
int read_group_desc(struct oscdfs_group_desc *desc)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: read_group_desc: malloc failed\n");
        return -1;
    }

    if (read_block(OSCDFS_GROUP_DESC_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    memcpy(desc, buf, sizeof(struct oscdfs_group_desc));
    free(buf);
    return 0;
}

/* 将组描述符写回磁盘 */
int write_group_desc(const struct oscdfs_group_desc *desc)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: write_group_desc: malloc failed\n");
        return -1;
    }

    if (read_block(OSCDFS_GROUP_DESC_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    memcpy(buf, desc, sizeof(struct oscdfs_group_desc));

    if (write_block(OSCDFS_GROUP_DESC_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    free(buf);
    return 0;
}

/* 原子更新组描述符中的空闲块和空闲 inode 计数 */
int group_desc_update_counts(int delta_blocks, int delta_inodes)
{
    struct oscdfs_group_desc desc;

    if (read_group_desc(&desc) != 0)
        return -1;

    if ((int)desc.free_blocks_count + delta_blocks < 0 ||
        (int)desc.free_blocks_count + delta_blocks > (int)OSCDFS_TOTAL_BLOCKS) {
        fprintf(stderr, "oscdfs: group_desc_update_counts: free_blocks overflow\n");
        return -1;
    }
    if ((int)desc.free_inodes_count + delta_inodes < 0 ||
        (int)desc.free_inodes_count + delta_inodes > (int)OSCDFS_TOTAL_INODES) {
        fprintf(stderr, "oscdfs: group_desc_update_counts: free_inodes overflow\n");
        return -1;
    }

    desc.free_blocks_count += delta_blocks;
    desc.free_inodes_count += delta_inodes;

    return write_group_desc(&desc);
}