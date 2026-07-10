// oscdfs/src/file.c
#include "oscdfs.h"

static int split_path(const char *fullpath, char *parent_dir, char *filename)
{
    if (fullpath == NULL || strlen(fullpath) == 0) return -1;
    const char *last_slash = strrchr(fullpath, '/');
    if (last_slash == NULL) {
        strcpy(parent_dir, ".");
        strncpy(filename, fullpath, OSCDFS_MAX_FILENAME - 1);
        filename[OSCDFS_MAX_FILENAME - 1] = '\0';
    } else {
        if (last_slash == fullpath) {
            if (strlen(fullpath) == 1) return -1;
            strcpy(parent_dir, "/");
        } else {
            size_t len = last_slash - fullpath;
            if (len >= MAX_CMD_LEN) return -1;
            strncpy(parent_dir, fullpath, len);
            parent_dir[len] = '\0';
        }
        strncpy(filename, last_slash + 1, OSCDFS_MAX_FILENAME - 1);
        filename[OSCDFS_MAX_FILENAME - 1] = '\0';
    }
    if (strlen(filename) == 0) return -1;
    return 0;
}

int alloc_fd(void)
{
    for (int i = 0; i < OSCDFS_MAX_OPEN_FILES; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = 1;
            fd_table[i].inode_no = 0;
            fd_table[i].offset = 0;
            fd_table[i].mode = 0;
            return i;
        }
    }
    fprintf(stderr, "oscdfs: alloc_fd: no free file descriptor\n");
    return -1;
}

void free_fd(int fd)
{
    if (fd < 0 || fd >= OSCDFS_MAX_OPEN_FILES) return;
    if (!fd_table[fd].used) return;
    fd_table[fd].used = 0;
    fd_table[fd].inode_no = 0;
    fd_table[fd].offset = 0;
    fd_table[fd].mode = 0;
}

int oscdfs_create(const char *path)
{
    char parent_dir[MAX_CMD_LEN];
    char filename[OSCDFS_MAX_FILENAME];
    if (split_path(path, parent_dir, filename) != 0) {
        fprintf(stderr, "oscdfs: oscdfs_create: invalid path '%s'\n", path);
        return -1;
    }

    uint32_t parent_ino = find_inode_by_path(parent_dir, current_dir_inode);
    if (parent_ino == (uint32_t)-1) {
        fprintf(stderr, "oscdfs: oscdfs_create: parent '%s' not found\n", parent_dir);
        return -1;
    }
    struct oscdfs_inode pinode;
    if (read_inode(parent_ino, &pinode) != 0 || !(pinode.mode & OSCDFS_S_IFDIR)) {
        fprintf(stderr, "oscdfs: oscdfs_create: '%s' not a directory\n", parent_dir);
        return -1;
    }
    if (dir_find_entry(parent_ino, filename) >= 0) {
        fprintf(stderr, "oscdfs: oscdfs_create: '%s' already exists\n", path);
        return -1;
    }
    int new_ino = alloc_inode();
    if (new_ino < 0) return -1;
    inode_init((uint32_t)new_ino, OSCDFS_DEFAULT_FILE_MODE, current_uid, current_gid);
    if (dir_add_entry(parent_ino, filename, (uint32_t)new_ino) != 0) {
        free_inode((uint32_t)new_ino);
        return -1;
    }
    return new_ino;
}

int oscdfs_open(const char *path, int flags)
{
    if (path == NULL) return -1;
    uint32_t ino = find_inode_by_path(path, current_dir_inode);
    if (ino == (uint32_t)-1) {
        fprintf(stderr, "oscdfs: oscdfs_open: '%s' not found\n", path);
        return -1;
    }
    struct oscdfs_inode inode;
    if (read_inode(ino, &inode) != 0) return -1;
    if (inode.mode & OSCDFS_S_IFDIR) {
        fprintf(stderr, "oscdfs: oscdfs_open: '%s' is a directory\n", path);
        return -1;
    }
    int fd = alloc_fd();
    if (fd < 0) return -1;
    fd_table[fd].inode_no = ino;
    fd_table[fd].offset = 0;
    fd_table[fd].mode = (uint32_t)flags;
    return fd;
}

int oscdfs_close(int fd)
{
    if (fd < 0 || fd >= OSCDFS_MAX_OPEN_FILES || !fd_table[fd].used) {
        fprintf(stderr, "oscdfs: oscdfs_close: invalid fd %d\n", fd);
        return -1;
    }
    free_fd(fd);
    return 0;
}

int oscdfs_read(int fd, void *buf, uint32_t nbytes)
{
    if (fd < 0 || fd >= OSCDFS_MAX_OPEN_FILES || !fd_table[fd].used) return -1;
    if (fd_table[fd].mode == OSCDFS_O_WRONLY) {
        fprintf(stderr, "oscdfs: oscdfs_read: fd %d is write-only\n", fd);
        return -1;
    }
    uint32_t ino = fd_table[fd].inode_no;
    struct oscdfs_inode inode;
    if (read_inode(ino, &inode) != 0) return -1;

    uint32_t total = 0, remaining = nbytes, cur = fd_table[fd].offset;
    if (cur >= inode.size) return 0;

    uint8_t *block_buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!block_buf) return -1;

    while (remaining > 0 && cur < inode.size) {
        uint32_t blk = cur / OSCDFS_BLOCK_SIZE, off = cur % OSCDFS_BLOCK_SIZE;
        int phys = inode_get_block(&inode, blk, 0);
        uint32_t chunk = remaining;
        if (chunk > OSCDFS_BLOCK_SIZE - off) chunk = OSCDFS_BLOCK_SIZE - off;
        if (cur + chunk > inode.size) chunk = inode.size - cur;

        if (phys >= 0) {
            if (read_block((uint32_t)phys, block_buf) != OSCDFS_BLOCK_SIZE) { free(block_buf); return -1; }
            memcpy((uint8_t*)buf + total, block_buf + off, chunk);
        } else {
            memset((uint8_t*)buf + total, 0, chunk);
        }
        total += chunk; remaining -= chunk; cur += chunk;
    }
    fd_table[fd].offset = cur;
    free(block_buf);
    return total;
}

int oscdfs_write(int fd, const void *buf, uint32_t nbytes)
{
    if (fd < 0 || fd >= OSCDFS_MAX_OPEN_FILES || !fd_table[fd].used) return -1;
    if (fd_table[fd].mode == OSCDFS_O_RDONLY) {
        fprintf(stderr, "oscdfs: oscdfs_write: fd %d is read-only\n", fd);
        return -1;
    }
    uint32_t ino = fd_table[fd].inode_no;
    struct oscdfs_inode inode;
    if (read_inode(ino, &inode) != 0) return -1;

    uint32_t total = 0, remaining = nbytes, cur = fd_table[fd].offset;
    uint8_t *block_buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!block_buf) return -1;

    while (remaining > 0) {
        uint32_t blk = cur / OSCDFS_BLOCK_SIZE, off = cur % OSCDFS_BLOCK_SIZE;
        int phys = inode_get_block(&inode, blk, 1);
        if (phys < 0) break;

        uint32_t chunk = remaining;
        if (chunk > OSCDFS_BLOCK_SIZE - off) chunk = OSCDFS_BLOCK_SIZE - off;

        if (chunk < OSCDFS_BLOCK_SIZE || off != 0) {
            if (read_block((uint32_t)phys, block_buf) != OSCDFS_BLOCK_SIZE) { free(block_buf); return -1; }
        } else {
            memset(block_buf, 0, OSCDFS_BLOCK_SIZE);
        }
        memcpy(block_buf + off, (const uint8_t*)buf + total, chunk);
        if (write_block((uint32_t)phys, block_buf) != OSCDFS_BLOCK_SIZE) { free(block_buf); return -1; }

        total += chunk; remaining -= chunk; cur += chunk;
    }

    if (cur > inode.size) inode.size = cur;
    inode.mtime = (uint32_t)time(NULL);
    fd_table[fd].offset = cur;
    write_inode(ino, &inode);
    free(block_buf);
    return total;
}

int oscdfs_delete(const char *path)
{
    char parent_dir[MAX_CMD_LEN], filename[OSCDFS_MAX_FILENAME];
    if (split_path(path, parent_dir, filename) != 0) return -1;
    uint32_t pino = find_inode_by_path(parent_dir, current_dir_inode);
    if (pino == (uint32_t)-1) return -1;

    struct oscdfs_inode pinode;
    if (read_inode(pino, &pinode) != 0 || !(pinode.mode & OSCDFS_S_IFDIR)) return -1;

    int fino = dir_find_entry(pino, filename);
    if (fino < 0) {
        fprintf(stderr, "oscdfs: oscdfs_delete: not found\n");
        return -1;
    }

    struct oscdfs_inode finode;
    if (read_inode((uint32_t)fino, &finode) != 0) return -1;
    if (finode.mode & OSCDFS_S_IFDIR) {
        fprintf(stderr, "oscdfs: oscdfs_delete: is a directory\n");
        return -1;
    }

    /* 检查是否有任何文件描述符正在使用该 inode */
    for (int i = 0; i < OSCDFS_MAX_OPEN_FILES; i++) {
        if (fd_table[i].used && fd_table[i].inode_no == (uint32_t)fino) {
            fprintf(stderr, "oscdfs: oscdfs_delete: file is open (fd %d)\n", i);
            return -1;
        }
    }

    if (dir_remove_entry(pino, filename) != 0) return -1;
    inode_free_blocks((uint32_t)fino, &finode);
    free_inode((uint32_t)fino);
    return 0;
}