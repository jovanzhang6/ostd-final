// oscdfs/src/fuse.c
#include "oscdfs.h"

/* FUSE 回调函数（静态，内部使用） */
static int fuse_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));
    oscdfs_set_user_from_fuse_context();

    uint32_t ino = find_inode_by_path(path, 1);
    if (ino == (uint32_t)-1)
        return -ENOENT;

    struct oscdfs_inode inode;
    if (read_inode(ino, &inode) != 0)
        return -EIO;

    stbuf->st_ino     = ino;
    stbuf->st_mode    = inode.mode & 07777;
    if (inode.mode & OSCDFS_S_IFDIR)
        stbuf->st_mode |= S_IFDIR;
    else
        stbuf->st_mode |= S_IFREG;
    stbuf->st_nlink   = 2;
    stbuf->st_uid     = inode.uid;
    stbuf->st_gid     = inode.gid;
    stbuf->st_size    = inode.size;
    stbuf->st_atime   = inode.atime;
    stbuf->st_mtime   = inode.mtime;
    stbuf->st_ctime   = inode.ctime;
    stbuf->st_blksize = OSCDFS_BLOCK_SIZE;
    stbuf->st_blocks  = (inode.size + 511) / 512;

    return 0;
}
static int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi)
{
    (void)offset;
    (void)fi;
    oscdfs_set_user_from_fuse_context();

    uint32_t ino = find_inode_by_path(path, 1);
    if (ino == (uint32_t)-1)
        return -ENOENT;

    struct oscdfs_inode dir_inode;
    if (read_inode(ino, &dir_inode) != 0)
        return -EIO;
    if (!(dir_inode.mode & OSCDFS_S_IFDIR))
        return -ENOTDIR;

    uint8_t *block_buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!block_buf) return -ENOMEM;

    uint32_t logical = 0;
    while (1) {
        int phys = inode_get_block(&dir_inode, logical, 0);
        if (phys < 0) break;
        if (read_block((uint32_t)phys, block_buf) != OSCDFS_BLOCK_SIZE) {
            free(block_buf);
            return -EIO;
        }
        struct oscdfs_dir_entry *entries = (struct oscdfs_dir_entry *)block_buf;
        for (int i = 0; i < OSCDFS_DIR_ENTRIES_PER_BLOCK; i++) {
            if (entries[i].inode_no != 0)
                filler(buf, entries[i].name, NULL, 0);
        }
        logical++;
    }
    free(block_buf);
    return 0;
}

static int fuse_open(const char *path, struct fuse_file_info *fi)
{
    oscdfs_set_user_from_fuse_context();
    uint32_t ino = find_inode_by_path(path, 1);
    if (ino == (uint32_t)-1)
        return -ENOENT;

    struct oscdfs_inode inode;
    if (read_inode(ino, &inode) != 0)
        return -EIO;
    if (inode.mode & OSCDFS_S_IFDIR)
        return -EISDIR;

    int acc = 0;
    if (fi->flags & O_RDONLY) acc = OSCDFS_R_OK;
    else if (fi->flags & O_WRONLY) acc = OSCDFS_W_OK;
    else if (fi->flags & O_RDWR) acc = OSCDFS_R_OK | OSCDFS_W_OK;
    if (oscdfs_check_permission(&inode, (uint32_t)acc) != 0)
        return -EACCES;

    fi->fh = ino;
    return 0;
}

static int fuse_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    (void)path;
    uint32_t ino = (uint32_t)fi->fh;
    int ret = oscdfs_read_inode(ino, buf, offset, size);
    if (ret < 0) return -errno;
    return ret;
}

static int fuse_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
    (void)path;
    oscdfs_set_user_from_fuse_context();

    uint32_t ino = (uint32_t)fi->fh;

    struct oscdfs_inode inode;
    if (read_inode(ino, &inode) != 0) return -EIO;
    if (oscdfs_check_permission(&inode, OSCDFS_W_OK) != 0)
        return -EACCES;

    int ret = oscdfs_write_inode(ino, buf, offset, size);
    if (ret < 0) return -errno;
    return ret;
}

static int fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    oscdfs_set_user_from_fuse_context();

    char parent_dir[MAX_CMD_LEN];
    char filename[OSCDFS_MAX_FILENAME];
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL || last_slash == path) {
        strcpy(parent_dir, "/");
        strncpy(filename, path + 1, OSCDFS_MAX_FILENAME - 1);
    } else {
        size_t len = last_slash - path;
        strncpy(parent_dir, path, len);
        parent_dir[len] = '\0';
        strncpy(filename, last_slash + 1, OSCDFS_MAX_FILENAME - 1);
    }
    filename[OSCDFS_MAX_FILENAME - 1] = '\0';

    uint32_t parent_ino = find_inode_by_path(parent_dir, 1);
    if (parent_ino == (uint32_t)-1)
        return -ENOENT;

    struct oscdfs_inode pinode;
    if (read_inode(parent_ino, &pinode) != 0 || !(pinode.mode & OSCDFS_S_IFDIR))
        return -ENOTDIR;
    if (oscdfs_check_permission(&pinode, OSCDFS_W_OK) != 0)
        return -EACCES;

    if (dir_find_entry(parent_ino, filename) >= 0)
        return -EEXIST;

    int new_ino = alloc_inode();
    if (new_ino < 0) return -ENOSPC;

    inode_init((uint32_t)new_ino, OSCDFS_DEFAULT_FILE_MODE, current_uid, current_gid);
    struct oscdfs_inode new_inode;
    read_inode((uint32_t)new_ino, &new_inode);
    new_inode.mode = (new_inode.mode & ~07777) | (mode & 07777);
    write_inode((uint32_t)new_ino, &new_inode);

    if (dir_add_entry(parent_ino, filename, (uint32_t)new_ino) != 0) {
        free_inode((uint32_t)new_ino);
        return -EIO;
    }

    fi->fh = (uint32_t)new_ino;
    return 0;
}

static int fuse_unlink(const char *path)
{
    oscdfs_set_user_from_fuse_context();
    if (oscdfs_delete(path) != 0)
        return -EACCES;
    return 0;
}

static int fuse_mkdir(const char *path, mode_t mode)
{
    oscdfs_set_user_from_fuse_context();

    char parent_dir[MAX_CMD_LEN];
    char dirname[OSCDFS_MAX_FILENAME];
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL || last_slash == path) {
        strcpy(parent_dir, "/");
        strncpy(dirname, path + 1, OSCDFS_MAX_FILENAME - 1);
    } else {
        size_t len = last_slash - path;
        strncpy(parent_dir, path, len);
        parent_dir[len] = '\0';
        strncpy(dirname, last_slash + 1, OSCDFS_MAX_FILENAME - 1);
    }
    dirname[OSCDFS_MAX_FILENAME - 1] = '\0';

    uint32_t parent_ino = find_inode_by_path(parent_dir, 1);
    if (parent_ino == (uint32_t)-1)
        return -ENOENT;

    struct oscdfs_inode pinode;
    if (read_inode(parent_ino, &pinode) != 0 || !(pinode.mode & OSCDFS_S_IFDIR))
        return -ENOTDIR;
    if (oscdfs_check_permission(&pinode, OSCDFS_W_OK) != 0)
        return -EACCES;
    if (dir_find_entry(parent_ino, dirname) >= 0)
        return -EEXIST;

    int new_ino = alloc_inode();
    if (new_ino < 0) return -ENOSPC;

    uint32_t dir_mode = OSCDFS_S_IFDIR | (mode & 07777);
    inode_init((uint32_t)new_ino, dir_mode, current_uid, current_gid);

    struct oscdfs_inode new_inode;
    read_inode((uint32_t)new_ino, &new_inode);
    int blk = inode_get_block(&new_inode, 0, 1);
    if (blk < 0) {
        free_inode((uint32_t)new_ino);
        return -ENOSPC;
    }
    uint8_t *buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!buf) {
        free_inode((uint32_t)new_ino);
        return -ENOMEM;
    }
    memset(buf, 0, OSCDFS_BLOCK_SIZE);
    struct oscdfs_dir_entry *e = (struct oscdfs_dir_entry *)buf;
    e[0].inode_no = (uint32_t)new_ino;
    strncpy(e[0].name, ".", 27);
    e[1].inode_no = parent_ino;
    strncpy(e[1].name, "..", 27);
    write_block((uint32_t)blk, buf);
    free(buf);

    new_inode.size = OSCDFS_BLOCK_SIZE;
    new_inode.mtime = (uint32_t)time(NULL);
    write_inode((uint32_t)new_ino, &new_inode);

    if (dir_add_entry(parent_ino, dirname, (uint32_t)new_ino) != 0) {
        inode_free_blocks((uint32_t)new_ino, &new_inode);
        free_inode((uint32_t)new_ino);
        return -EIO;
    }
    return 0;
}

static int fuse_rmdir(const char *path)
{
    oscdfs_set_user_from_fuse_context();
    uint32_t ino = find_inode_by_path(path, 1);
    if (ino == (uint32_t)-1) return -ENOENT;

    struct oscdfs_inode inode;
    if (read_inode(ino, &inode) != 0) return -EIO;
    if (!(inode.mode & OSCDFS_S_IFDIR)) return -ENOTDIR;

    uint8_t *block_buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!block_buf) return -ENOMEM;
    int empty = 1;
    uint32_t logical = 0;
    while (1) {
        int phys = inode_get_block(&inode, logical, 0);
        if (phys < 0) break;
        if (read_block((uint32_t)phys, block_buf) != OSCDFS_BLOCK_SIZE) {
            free(block_buf); return -EIO;
        }
        struct oscdfs_dir_entry *entries = (struct oscdfs_dir_entry *)block_buf;
        for (int i = 0; i < OSCDFS_DIR_ENTRIES_PER_BLOCK; i++) {
            if (entries[i].inode_no != 0 &&
                strcmp(entries[i].name, ".") != 0 &&
                strcmp(entries[i].name, "..") != 0) {
                empty = 0;
                break;
            }
        }
        if (!empty) break;
        logical++;
    }
    free(block_buf);
    if (!empty) return -ENOTEMPTY;

    /* 提取 basename（要删除的目录名） */
    const char *last_slash_orig = strrchr(path, '/');
    if (last_slash_orig == NULL) return -EIO;
    const char *del_name = last_slash_orig + 1;

    /* 提取 dirname（父目录路径） */
    char parent_dir[MAX_CMD_LEN];
    strncpy(parent_dir, path, MAX_CMD_LEN - 1);
    parent_dir[MAX_CMD_LEN - 1] = '\0';
    char *last_slash = strrchr(parent_dir, '/');
    if (last_slash == NULL) return -EIO;
    if (last_slash == parent_dir) strcpy(parent_dir, "/");
    else *last_slash = '\0';

    uint32_t parent_ino = find_inode_by_path(parent_dir, 1);
    if (parent_ino == (uint32_t)-1) return -ENOENT;
    struct oscdfs_inode pinode;
    if (read_inode(parent_ino, &pinode) != 0) return -EIO;
    if (oscdfs_check_permission(&pinode, OSCDFS_W_OK) != 0) return -EACCES;

    if (dir_remove_entry(parent_ino, del_name) != 0) return -EIO;
    inode_free_blocks(ino, &inode);
    free_inode(ino);
    return 0;
}

static int fuse_chmod(const char *path, mode_t mode)
{
    oscdfs_set_user_from_fuse_context();
    uint32_t ino = find_inode_by_path(path, 1);
    if (ino == (uint32_t)-1) return -ENOENT;

    struct oscdfs_inode inode;
    if (read_inode(ino, &inode) != 0) return -EIO;
    if (oscdfs_check_permission(&inode, 0) != 0 && current_uid != 0 && current_uid != inode.uid)
        return -EACCES;

    inode.mode = (inode.mode & ~07777) | (mode & 07777);
    inode.mtime = (uint32_t)time(NULL);
    if (write_inode(ino, &inode) != 0) return -EIO;
    return 0;
}

static int fuse_chown(const char *path, uid_t uid, gid_t gid)
{
    oscdfs_set_user_from_fuse_context();
    if (current_uid != 0) return -EACCES;

    uint32_t ino = find_inode_by_path(path, 1);
    if (ino == (uint32_t)-1) return -ENOENT;

    struct oscdfs_inode inode;
    if (read_inode(ino, &inode) != 0) return -EIO;

    if (uid != (uid_t)-1) {
        struct oscdfs_user users[OSCDFS_MAX_USERS];
        if (read_user_table(users) == 0) {
            int found = 0;
            for (int i = 0; i < OSCDFS_MAX_USERS; i++) {
                if (users[i].username[0] != '\0' && users[i].linux_uid == (uint32_t)uid) {
                    inode.uid = users[i].uid;
                    found = 1;
                    break;
                }
            }
            if (!found) return -EINVAL;
        }
    }
    if (gid != (gid_t)-1) {
        inode.gid = (uint32_t)gid;
    }
    inode.mtime = (uint32_t)time(NULL);
    if (write_inode(ino, &inode) != 0) return -EIO;
    return 0;
}

static int fuse_utimens(const char *path, const struct timespec tv[2])
{
    oscdfs_set_user_from_fuse_context();
    uint32_t ino = find_inode_by_path(path, 1);
    if (ino == (uint32_t)-1) return -ENOENT;

    struct oscdfs_inode inode;
    if (read_inode(ino, &inode) != 0) return -EIO;

    if (tv[0].tv_nsec == UTIME_NOW)
        inode.atime = (uint32_t)time(NULL);
    else if (tv[0].tv_nsec != UTIME_OMIT)
        inode.atime = (uint32_t)tv[0].tv_sec;

    if (tv[1].tv_nsec == UTIME_NOW)
        inode.mtime = (uint32_t)time(NULL);
    else if (tv[1].tv_nsec != UTIME_OMIT)
        inode.mtime = (uint32_t)tv[1].tv_sec;

    if (write_inode(ino, &inode) != 0) return -EIO;
    return 0;
}

/* FUSE 操作结构体，使用新的静态函数名 */
static struct fuse_operations oscdfs_oper = {
    .getattr = fuse_getattr,
    .readdir = fuse_readdir,
    .open    = fuse_open,
    .read    = fuse_read,
    .write   = fuse_write,
    .create  = fuse_create,
    .unlink  = fuse_unlink,
    .mkdir   = fuse_mkdir,
    .rmdir   = fuse_rmdir,
    .chmod   = fuse_chmod,
    .chown   = fuse_chown,
    .utimens = fuse_utimens,
};

int oscdfs_fuse_main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &oscdfs_oper, NULL);
}