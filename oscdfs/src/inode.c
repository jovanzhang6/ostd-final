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

    uint32_t offset = inode_no * sizeof(struct oscdfs_inode);
    uint32_t blk_index = offset / OSCDFS_BLOCK_SIZE;
    uint32_t blk_off   = offset % OSCDFS_BLOCK_SIZE;

    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: read_inode: malloc failed\n");
        return -1;
    }

    if (read_block(INODE_TABLE_START + blk_index, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }

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
    inode.atime = inode.ctime;
    inode.mtime = inode.ctime;

    if (write_inode(inode_no, &inode) != 0) {
        fprintf(stderr, "oscdfs: inode_init: write failed\n");
        exit(EXIT_FAILURE);
    }
}

int inode_get_block(struct oscdfs_inode *inode, uint32_t logical_block, int allocate)
{
    /* ---------- 直接块 ---------- */
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

    /* ---------- 单间接块 ---------- */
    if (logical_block < 12 + OSCDFS_INDIRECT_POINTERS) {
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

    /* ---------- 双间接块 ---------- */
    if (logical_block < 12 + OSCDFS_INDIRECT_POINTERS + OSCDFS_INDIRECT_POINTERS * OSCDFS_INDIRECT_POINTERS) {
        uint32_t offset = logical_block - 12 - OSCDFS_INDIRECT_POINTERS;
        uint32_t dblk_idx = offset / OSCDFS_INDIRECT_POINTERS;
        uint32_t blk_idx  = offset % OSCDFS_INDIRECT_POINTERS;

        if (inode->double_indirect == 0) {
            if (!allocate)
                return -1;
            int new_blk = alloc_block();
            if (new_blk < 0)
                return -1;
            inode->double_indirect = (uint32_t)new_blk;

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

        uint32_t *double_block = malloc(OSCDFS_BLOCK_SIZE);
        if (!double_block) {
            fprintf(stderr, "oscdfs: inode_get_block: malloc failed\n");
            return -1;
        }
        if (read_block(inode->double_indirect, double_block) != OSCDFS_BLOCK_SIZE) {
            free(double_block);
            return -1;
        }

        if (double_block[dblk_idx] == 0) {
            if (!allocate) {
                free(double_block);
                return -1;
            }
            int new_blk = alloc_block();
            if (new_blk < 0) {
                free(double_block);
                return -1;
            }
            double_block[dblk_idx] = (uint32_t)new_blk;

            uint8_t *zero_buf = malloc(OSCDFS_BLOCK_SIZE);
            if (!zero_buf) {
                free(double_block);
                return -1;
            }
            memset(zero_buf, 0, OSCDFS_BLOCK_SIZE);
            if (write_block((uint32_t)new_blk, zero_buf) != OSCDFS_BLOCK_SIZE) {
                free(zero_buf);
                free(double_block);
                return -1;
            }
            free(zero_buf);

            if (write_block(inode->double_indirect, double_block) != OSCDFS_BLOCK_SIZE) {
                free(double_block);
                return -1;
            }
        }

        uint32_t *indirect = malloc(OSCDFS_BLOCK_SIZE);
        if (!indirect) {
            fprintf(stderr, "oscdfs: inode_get_block: malloc failed\n");
            free(double_block);
            return -1;
        }
        if (read_block(double_block[dblk_idx], indirect) != OSCDFS_BLOCK_SIZE) {
            free(indirect);
            free(double_block);
            return -1;
        }

        if (indirect[blk_idx] != 0) {
            int blk = (int)indirect[blk_idx];
            free(indirect);
            free(double_block);
            return blk;
        }

        if (!allocate) {
            free(indirect);
            free(double_block);
            return -1;
        }

        int new_blk = alloc_block();
        if (new_blk < 0) {
            free(indirect);
            free(double_block);
            return -1;
        }
        indirect[blk_idx] = (uint32_t)new_blk;

        if (write_block(double_block[dblk_idx], indirect) != OSCDFS_BLOCK_SIZE) {
            free(indirect);
            free(double_block);
            return -1;
        }

        free(indirect);
        free(double_block);
        return new_blk;
    }

    /* ---------- 三间接块 ---------- */
    {
        uint32_t offset = logical_block - 12 - OSCDFS_INDIRECT_POINTERS
                          - OSCDFS_INDIRECT_POINTERS * OSCDFS_INDIRECT_POINTERS;
        uint32_t idx1 = offset / (OSCDFS_INDIRECT_POINTERS * OSCDFS_INDIRECT_POINTERS);
        uint32_t idx2 = (offset / OSCDFS_INDIRECT_POINTERS) % OSCDFS_INDIRECT_POINTERS;
        uint32_t idx3 = offset % OSCDFS_INDIRECT_POINTERS;

        if (inode->triple_indirect == 0) {
            if (!allocate)
                return -1;
            int new_blk = alloc_block();
            if (new_blk < 0)
                return -1;
            inode->triple_indirect = (uint32_t)new_blk;

            uint8_t *zero = malloc(OSCDFS_BLOCK_SIZE);
            if (!zero) return -1;
            memset(zero, 0, OSCDFS_BLOCK_SIZE);
            if (write_block((uint32_t)new_blk, zero) != OSCDFS_BLOCK_SIZE) {
                free(zero);
                return -1;
            }
            free(zero);
        }

        uint32_t *triple_block = malloc(OSCDFS_BLOCK_SIZE);
        if (!triple_block) return -1;
        if (read_block(inode->triple_indirect, triple_block) != OSCDFS_BLOCK_SIZE) {
            free(triple_block);
            return -1;
        }

        if (triple_block[idx1] == 0) {
            if (!allocate) { free(triple_block); return -1; }
            int blk = alloc_block();
            if (blk < 0) { free(triple_block); return -1; }
            triple_block[idx1] = (uint32_t)blk;

            uint8_t *zero = malloc(OSCDFS_BLOCK_SIZE);
            if (!zero) { free(triple_block); return -1; }
            memset(zero, 0, OSCDFS_BLOCK_SIZE);
            if (write_block((uint32_t)blk, zero) != OSCDFS_BLOCK_SIZE) {
                free(zero); free(triple_block); return -1;
            }
            free(zero);

            if (write_block(inode->triple_indirect, triple_block) != OSCDFS_BLOCK_SIZE) {
                free(triple_block); return -1;
            }
        }

        uint32_t *double_block = malloc(OSCDFS_BLOCK_SIZE);
        if (!double_block) { free(triple_block); return -1; }
        if (read_block(triple_block[idx1], double_block) != OSCDFS_BLOCK_SIZE) {
            free(double_block); free(triple_block); return -1;
        }

        if (double_block[idx2] == 0) {
            if (!allocate) { free(double_block); free(triple_block); return -1; }
            int blk = alloc_block();
            if (blk < 0) { free(double_block); free(triple_block); return -1; }
            double_block[idx2] = (uint32_t)blk;

            uint8_t *zero = malloc(OSCDFS_BLOCK_SIZE);
            if (!zero) { free(double_block); free(triple_block); return -1; }
            memset(zero, 0, OSCDFS_BLOCK_SIZE);
            if (write_block((uint32_t)blk, zero) != OSCDFS_BLOCK_SIZE) {
                free(zero); free(double_block); free(triple_block); return -1;
            }
            free(zero);

            if (write_block(triple_block[idx1], double_block) != OSCDFS_BLOCK_SIZE) {
                free(double_block); free(triple_block); return -1;
            }
        }

        uint32_t *indirect = malloc(OSCDFS_BLOCK_SIZE);
        if (!indirect) { free(double_block); free(triple_block); return -1; }
        if (read_block(double_block[idx2], indirect) != OSCDFS_BLOCK_SIZE) {
            free(indirect); free(double_block); free(triple_block); return -1;
        }

        if (indirect[idx3] != 0) {
            int data_blk = (int)indirect[idx3];
            free(indirect); free(double_block); free(triple_block);
            return data_blk;
        }

        if (!allocate) {
            free(indirect); free(double_block); free(triple_block);
            return -1;
        }

        int new_data = alloc_block();
        if (new_data < 0) { free(indirect); free(double_block); free(triple_block); return -1; }
        indirect[idx3] = (uint32_t)new_data;

        if (write_block(double_block[idx2], indirect) != OSCDFS_BLOCK_SIZE) {
            free(indirect); free(double_block); free(triple_block); return -1;
        }

        free(indirect);
        free(double_block);
        free(triple_block);
        return new_data;
    }
}

void inode_free_blocks(uint32_t inode_no, struct oscdfs_inode *inode)
{
    /* 释放直接块 */
    for (int i = 0; i < 12; i++) {
        if (inode->blocks[i] != 0) {
            free_block(inode->blocks[i]);
            inode->blocks[i] = 0;
        }
    }

    /* 释放单间接块及其指向的数据块 */
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

    /* 释放双间接块及其所有子块 */
    if (inode->double_indirect != 0) {
        uint32_t *double_block = malloc(OSCDFS_BLOCK_SIZE);
        if (double_block) {
            if (read_block(inode->double_indirect, double_block) == OSCDFS_BLOCK_SIZE) {
                for (uint32_t i = 0; i < OSCDFS_INDIRECT_POINTERS; i++) {
                    if (double_block[i] != 0) {
                        uint32_t *indirect_block = malloc(OSCDFS_BLOCK_SIZE);
                        if (indirect_block) {
                            if (read_block(double_block[i], indirect_block) == OSCDFS_BLOCK_SIZE) {
                                for (uint32_t j = 0; j < OSCDFS_INDIRECT_POINTERS; j++) {
                                    if (indirect_block[j] != 0) {
                                        free_block(indirect_block[j]);
                                    }
                                }
                            }
                            free(indirect_block);
                        }
                        free_block(double_block[i]);   /* 释放二级间接块本身 */
                    }
                }
            }
            free(double_block);
        }
        free_block(inode->double_indirect);
        inode->double_indirect = 0;
    }

    /* 释放三间接块及其所有子块 */
    if (inode->triple_indirect != 0) {
        uint32_t *triple_block = malloc(OSCDFS_BLOCK_SIZE);
        if (triple_block) {
            if (read_block(inode->triple_indirect, triple_block) == OSCDFS_BLOCK_SIZE) {
                for (uint32_t i = 0; i < OSCDFS_INDIRECT_POINTERS; i++) {
                    if (triple_block[i] != 0) {
                        uint32_t *double_block = malloc(OSCDFS_BLOCK_SIZE);
                        if (double_block) {
                            if (read_block(triple_block[i], double_block) == OSCDFS_BLOCK_SIZE) {
                                for (uint32_t j = 0; j < OSCDFS_INDIRECT_POINTERS; j++) {
                                    if (double_block[j] != 0) {
                                        uint32_t *indirect_block = malloc(OSCDFS_BLOCK_SIZE);
                                        if (indirect_block) {
                                            if (read_block(double_block[j], indirect_block) == OSCDFS_BLOCK_SIZE) {
                                                for (uint32_t k = 0; k < OSCDFS_INDIRECT_POINTERS; k++) {
                                                    if (indirect_block[k] != 0)
                                                        free_block(indirect_block[k]);
                                                }
                                            }
                                            free(indirect_block);
                                        }
                                        free_block(double_block[j]);  /* 释放三级间接块本身 */
                                    }
                                }
                            }
                            free(double_block);
                        }
                        free_block(triple_block[i]);   /* 释放二级间接块本身 */
                    }
                }
            }
            free(triple_block);
        }
        free_block(inode->triple_indirect);
        inode->triple_indirect = 0;
    }

    inode->size = 0;
    write_inode(inode_no, inode);
}