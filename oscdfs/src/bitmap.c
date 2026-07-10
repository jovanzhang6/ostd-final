// oscdfs/src/bitmap.c
#include "oscdfs.h"

/* 位图所在的逻辑块号 */
#define BLOCK_BITMAP_BLOCK  1   /* 偏移 4096 */
#define INODE_BITMAP_BLOCK  2   /* 偏移 8192 */

/* 块位图初始化
 * 标记元数据区域（块 0 ~ 3）为已占用*/
void block_bitmap_init(void)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: block_bitmap_init: malloc failed\n");
        exit(EXIT_FAILURE);
    }
    memset(buf, 0, OSCDFS_BLOCK_SIZE);

    /* 块 0: 超级块, 块 1: 块位图, 块 2: inode 位图, 块 3: inode 表 */
    buf[0] |= 0x0F;   /* 低4位表示块0~3已被占用 */

    if (write_block(BLOCK_BITMAP_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        fprintf(stderr, "oscdfs: block_bitmap_init: write failed\n");
        free(buf);
        exit(EXIT_FAILURE);
    }
    free(buf);
}

/* inode 位图初始化
 * 全部空闲，不预占任何 inode*/
void inode_bitmap_init(void)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: inode_bitmap_init: malloc failed\n");
        exit(EXIT_FAILURE);
    }
    memset(buf, 0, OSCDFS_BLOCK_SIZE);

    /* 预占 inode 0（保留不用），确保 alloc_inode 从 1 开始分配 */
    buf[0] |= 0x01;

    if (write_block(INODE_BITMAP_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        fprintf(stderr, "oscdfs: inode_bitmap_init: write failed\n");
        free(buf);
        exit(EXIT_FAILURE);
    }
    free(buf);
}

/* 分配一个空闲数据块
 * 返回块号，失败返回 -1*/
int alloc_block(void)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: alloc_block: malloc failed\n");
        return -1;
    }

    if (read_block(BLOCK_BITMAP_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    uint32_t i;
    for (i = 0; i < OSCDFS_TOTAL_BLOCKS; i++) {
        uint32_t byte_idx = i / 8;
        uint8_t  mask     = (uint8_t)(1 << (i % 8));

        if ((buf[byte_idx] & mask) == 0) {
            buf[byte_idx] |= mask;

            if (write_block(BLOCK_BITMAP_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
                free(buf);
                return -1;
            }

            if (super_update_counts(-1, 0) != 0) {
                free(buf);
                return -1;
            }

            free(buf);
            return (int)i;
        }
    }

    fprintf(stderr, "oscdfs: alloc_block: no free block\n");
    free(buf);
    return -1;
}

/* 释放一个数据块
 * 成功返回 0，失败返回 -1*/
int free_block(uint32_t block_no)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: free_block: malloc failed\n");
        return -1;
    }

    if (block_no >= OSCDFS_TOTAL_BLOCKS) {
        fprintf(stderr, "oscdfs: free_block: block %u out of range\n", block_no);
        free(buf);
        return -1;
    }

    if (read_block(BLOCK_BITMAP_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    uint32_t byte_idx = block_no / 8;
    uint8_t  mask     = (uint8_t)(1 << (block_no % 8));

    if ((buf[byte_idx] & mask) == 0) {
        fprintf(stderr, "oscdfs: free_block: block %u already free\n", block_no);
        free(buf);
        return -1;
    }

    buf[byte_idx] &= ~mask;

    if (write_block(BLOCK_BITMAP_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    if (super_update_counts(1, 0) != 0) {
        free(buf);
        return -1;
    }

    free(buf);
    return 0;
}

/* 分配一个空闲 inode
 * 返回 inode 号，失败返回 -1*/
int alloc_inode(void)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: alloc_inode: malloc failed\n");
        return -1;
    }

    if (read_block(INODE_BITMAP_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    uint32_t i;
    for (i = 0; i < OSCDFS_TOTAL_INODES; i++) {
        uint32_t byte_idx = i / 8;
        uint8_t  mask     = (uint8_t)(1 << (i % 8));

        if ((buf[byte_idx] & mask) == 0) {
            buf[byte_idx] |= mask;

            if (write_block(INODE_BITMAP_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
                free(buf);
                return -1;
            }

            if (super_update_counts(0, -1) != 0) {
                free(buf);
                return -1;
            }

            free(buf);
            return (int)i;
        }
    }

    fprintf(stderr, "oscdfs: alloc_inode: no free inode\n");
    free(buf);
    return -1;
}

/* 释放一个 inode
 * 成功返回 0，失败返回 -1*/
int free_inode(uint32_t inode_no)
{
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: free_inode: malloc failed\n");
        return -1;
    }

    if (inode_no >= OSCDFS_TOTAL_INODES) {
        fprintf(stderr, "oscdfs: free_inode: inode %u out of range\n", inode_no);
        free(buf);
        return -1;
    }

    if (read_block(INODE_BITMAP_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    uint32_t byte_idx = inode_no / 8;
    uint8_t  mask     = (uint8_t)(1 << (inode_no % 8));

    if ((buf[byte_idx] & mask) == 0) {
        fprintf(stderr, "oscdfs: free_inode: inode %u already free\n", inode_no);
        free(buf);
        return -1;
    }

    buf[byte_idx] &= ~mask;

    if (write_block(INODE_BITMAP_BLOCK, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    if (super_update_counts(0, 1) != 0) {
        free(buf);
        return -1;
    }

    free(buf);
    return 0;
}