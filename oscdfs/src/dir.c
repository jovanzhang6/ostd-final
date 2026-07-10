// oscdfs/src/dir.c
#include "oscdfs.h"

int dir_init_root(uint32_t root_inode)
{
    struct oscdfs_inode inode;
    if (read_inode(root_inode, &inode) != 0)
        return -1;

    int blk = inode_get_block(&inode, 0, 1);
    if (blk < 0)
        return -1;

    /* 使用完整的块缓冲区 */
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        fprintf(stderr, "oscdfs: dir_init_root: malloc failed\n");
        return -1;
    }
    memset(buf, 0, OSCDFS_BLOCK_SIZE);

    struct oscdfs_dir_entry *entries = (struct oscdfs_dir_entry *)buf;
    entries[0].inode_no = root_inode;
    strncpy(entries[0].name, ".", 27);
    entries[0].name[27] = '\0';

    entries[1].inode_no = root_inode;
    strncpy(entries[1].name, "..", 27);
    entries[1].name[27] = '\0';

    if (write_block((uint32_t)blk, buf) != OSCDFS_BLOCK_SIZE) {
        free(buf);
        return -1;
    }
    free(buf);

    inode.size  = OSCDFS_BLOCK_SIZE;
    inode.mtime = (uint32_t)time(NULL);

    if (write_inode(root_inode, &inode) != 0)
        return -1;

    return 0;
}

int dir_add_entry(uint32_t dir_inode, const char *name, uint32_t entry_inode)
{
    struct oscdfs_inode inode;
    if (read_inode(dir_inode, &inode) != 0)
        return -1;

    if (!(inode.mode & OSCDFS_S_IFDIR)) {
        fprintf(stderr, "oscdfs: dir_add_entry: inode %u is not a directory\n", dir_inode);
        return -1;
    }

    char fname[28];
    strncpy(fname, name, 27);
    fname[27] = '\0';

    uint32_t logical_blk = 0;
    int found = 0;
    uint32_t target_block = 0;

    /* 使用动态分配的目录项块缓冲区 */
    struct oscdfs_dir_entry *block_entries = malloc(OSCDFS_BLOCK_SIZE);
    if (!block_entries) {
        fprintf(stderr, "oscdfs: dir_add_entry: malloc failed\n");
        return -1;
    }

    while (1) {
        int phys_blk = inode_get_block(&inode, logical_blk, 0);
        if (phys_blk < 0)
            break;

        if (read_block((uint32_t)phys_blk, block_entries) != OSCDFS_BLOCK_SIZE) {
            free(block_entries);
            return -1;
        }

        for (int i = 0; i < OSCDFS_DIR_ENTRIES_PER_BLOCK; i++) {
            if (block_entries[i].inode_no == 0) {
                block_entries[i].inode_no = entry_inode;
                strncpy(block_entries[i].name, fname, 28);
                target_block = (uint32_t)phys_blk;
                found = 1;
                break;
            }
        }
        if (found)
            break;
        logical_blk++;
    }

    if (!found) {
        int new_blk = inode_get_block(&inode, logical_blk, 1);
        if (new_blk < 0) {
            free(block_entries);
            return -1;
        }
        memset(block_entries, 0, OSCDFS_BLOCK_SIZE);
        block_entries[0].inode_no = entry_inode;
        strncpy(block_entries[0].name, fname, 28);
        target_block = (uint32_t)new_blk;
        inode.size += OSCDFS_BLOCK_SIZE;
    }

    if (write_block(target_block, block_entries) != OSCDFS_BLOCK_SIZE) {
        free(block_entries);
        return -1;
    }
    free(block_entries);

    inode.mtime = (uint32_t)time(NULL);
    if (write_inode(dir_inode, &inode) != 0)
        return -1;

    return 0;
}

int dir_find_entry(uint32_t dir_inode, const char *name)
{
    struct oscdfs_inode inode;
    if (read_inode(dir_inode, &inode) != 0)
        return -1;

    if (!(inode.mode & OSCDFS_S_IFDIR)) {
        fprintf(stderr, "oscdfs: dir_find_entry: inode %u is not a directory\n", dir_inode);
        return -1;
    }

    struct oscdfs_dir_entry *entries = malloc(OSCDFS_BLOCK_SIZE);
    if (!entries) {
        fprintf(stderr, "oscdfs: dir_find_entry: malloc failed\n");
        return -1;
    }

    uint32_t logical_blk = 0;
    while (1) {
        int phys_blk = inode_get_block(&inode, logical_blk, 0);
        if (phys_blk < 0)
            break;

        if (read_block((uint32_t)phys_blk, entries) != OSCDFS_BLOCK_SIZE) {
            free(entries);
            return -1;
        }

        for (int i = 0; i < OSCDFS_DIR_ENTRIES_PER_BLOCK; i++) {
            if (entries[i].inode_no != 0 &&
                strncmp(entries[i].name, name, 28) == 0) {
                int ret = (int)entries[i].inode_no;
                free(entries);
                return ret;
            }
        }
        logical_blk++;
    }

    free(entries);
    return -1;
}

uint32_t find_inode_by_path(const char *path, uint32_t start_inode)
{
    if (path == NULL)
        return (uint32_t)-1;

    char path_copy[MAX_CMD_LEN];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    uint32_t current_inode;
    char *saveptr;
    char *token;

    if (path_copy[0] == '/') {
        current_inode = 0;
    } else {
        current_inode = start_inode;
    }

    token = strtok_r(path_copy, "/", &saveptr);
    if (token == NULL) {
        return current_inode;
    }

    do {
        if (strcmp(token, ".") == 0) {
        } else if (strcmp(token, "..") == 0) {
            int parent = dir_find_entry(current_inode, "..");
            if (parent < 0) {
                return (uint32_t)-1;
            }
            current_inode = (uint32_t)parent;
        } else {
            int found = dir_find_entry(current_inode, token);
            if (found < 0) {
                return (uint32_t)-1;
            }
            current_inode = (uint32_t)found;
        }
        token = strtok_r(NULL, "/", &saveptr);
    } while (token != NULL);

    return current_inode;
}