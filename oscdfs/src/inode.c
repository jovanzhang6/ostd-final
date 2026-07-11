// oscdfs/src/inode.c
#include "oscdfs.h"

/*
 * Inode 表占用 4 个块：OSCDFS_INODE_TABLE_BLOCK .. OSCDFS_INODE_TABLE_BLOCK+3
 * 每个 inode 大小为 128 字节（定义在 oscdfs.h）。
 */
#define INODE_TABLE_START   OSCDFS_INODE_TABLE_BLOCK   /* 块4 */
#define INODE_TABLE_BLOCKS   4

int read_inode(uint32_t inode_no, struct oscdfs_inode *inode)
{
    if (inode_no >= OSCDFS_TOTAL_INODES) {
        fprintf(stderr, "oscdfs: read_inode: inode %u out of range\n", inode_no);
        return -1;
    }

    /* 计算字节偏移 */
    uint32_t offset = inode_no * sizeof(struct oscdfs_inode);
    /* 相对于 inode 表起始块，该 inode 所在的块索引 */
    uint32_t blk_index = offset / OSCDFS_BLOCK_SIZE;
    /* 块内偏移 */
    uint32_t blk_off   = offset % OSCDFS_BLOCK_SIZE;

    /* 读取对应的块 */
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: read_inode: malloc failed\n");
        return -1;
    }

    if (read_block(INODE_TABLE_START + blk_index, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    /* 从块中复制 inode 结构体 */
    memcpy(inode, buf + blk_off, sizeof(struct oscdfs_inode));
    free(buf);
    return 0;
}

int write_inode(uint32_t inode_no, const struct oscdfs_inode *inode)
{
    if (inode_no >= OSCDFS_TOTAL_INODES) {
        fprintf(stderr, "oscdfs: write_inode: inode %u out of range\n", inode_no);
        return -1;
    }

    uint32_t offset = inode_no * sizeof(struct oscdfs_inode);
    uint32_t blk_index = offset / OSCDFS_BLOCK_SIZE;
    uint32_t blk_off   = offset % OSCDFS_BLOCK_SIZE;

    /* 先读取整块，修改 inode 所在区域，再写回 */
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: write_inode: malloc failed\n");
        return -1;
    }

    if (read_block(INODE_TABLE_START + blk_index, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    memcpy(buf + blk_off, inode, sizeof(struct oscdfs_inode));

    if (write_block(INODE_TABLE_START + blk_index, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

    free(buf);
    return 0;
}

void inode_init(uint32_t inode_no, uint32_t mode, uint32_t uid, uint32_t gid)
{
    struct oscdfs_inode inode;
    memset(&inode, 0, sizeof(inode));

    inode.mode  = mode;
    inode.uid   = uid;
    inode.gid   = gid;
    inode.size  = 0;
    inode.ctime = (uint32_t)time(NULL);
    inode.atime = inode.ctime;   /* 初始访问时间等于创建时间 */
    inode.mtime = inode.ctime;

    if (write_inode(inode_no, &inode) != 0) {
        fprintf(stderr, "oscdfs: inode_init: write failed\n");
        exit(EXIT_FAILURE);
    }
}

int inode_get_block(struct oscdfs_inode *inode, uint32_t logical_block, int allocate)
{
    if (logical_block < 12) {
        if (inode->blocks[logical_block] != 0)
            return (int)inode->blocks[logical_block];
        if (!allocate)
            return -1;

        int new_blk = alloc_block();
        if (new_blk < 0)
            return -1;
        inode->blocks[logical_block] = (uint32_t)new_blk;
        return new_blk;
    }

    if (logical_block >= 12 + OSCDFS_INDIRECT_POINTERS) {
        fprintf(stderr, "oscdfs: inode_get_block: logical block %u exceeds max size\n", logical_block);
        return -1;
    }

    uint32_t indirect_idx = logical_block - 12;

    if (inode->indirect == 0) {
        if (!allocate)
            return -1;
        int new_blk = alloc_block();
        if (new_blk < 0)
            return -1;
        inode->indirect = (uint32_t)new_blk;

        uint8_t *zero_buf = malloc(OSCDFS_BLOCK_SIZE);
        if (!zero_buf) {
            fprintf(stderr, "oscdfs: inode_get_block: malloc failed\n");
            return -1;
        }
        memset(zero_buf, 0, OSCDFS_BLOCK_SIZE);
        if (write_block((uint32_t)new_blk, zero_buf) != OSCDFS_BLOCK_SIZE) {
            free(zero_buf);
            return -1;
        }
        free(zero_buf);
    }

    uint32_t *indirect_block = malloc(OSCDFS_BLOCK_SIZE);
    if (!indirect_block) {
        fprintf(stderr, "oscdfs: inode_get_block: malloc failed\n");
        return -1;
    }

    if (read_block(inode->indirect, indirect_block) != OSCDFS_BLOCK_SIZE) {
        free(indirect_block);
        return -1;
    }

    if (indirect_block[indirect_idx] != 0) {
        int blk = (int)indirect_block[indirect_idx];
        free(indirect_block);
        return blk;
    }

    if (!allocate) {
        free(indirect_block);
        return -1;
    }

    int new_blk = alloc_block();
    if (new_blk < 0) {
        free(indirect_block);
        return -1;
    }
    indirect_block[indirect_idx] = (uint32_t)new_blk;

    if (write_block(inode->indirect, indirect_block) != OSCDFS_BLOCK_SIZE) {
        free(indirect_block);
        return -1;
    }

    free(indirect_block);
    return new_blk;
}

void inode_free_blocks(uint32_t inode_no, struct oscdfs_inode *inode)
{
    for (int i = 0; i < 12; i++) {
        if (inode->blocks[i] != 0) {
            free_block(inode->blocks[i]);
            inode->blocks[i] = 0;
        }
    }

    if (inode->indirect != 0) {
        uint32_t *indirect_block = malloc(OSCDFS_BLOCK_SIZE);
        if (indirect_block) {
            if (read_block(inode->indirect, indirect_block) == OSCDFS_BLOCK_SIZE) {
                for (uint32_t i = 0; i < OSCDFS_INDIRECT_POINTERS; i++) {
                    if (indirect_block[i] != 0) {
                        free_block(indirect_block[i]);
                    }
                }
            }
            free(indirect_block);
        }
        free_block(inode->indirect);
        inode->indirect = 0;
    }

    inode->size = 0;
    write_inode(inode_no, inode);
}