// oscdfs/src/builtin.c
#include "oscdfs.h"

int builtin_hello(char **args) {
    (void)args;
    printf("Hello from oscdfs!\n");
    return 0;
}

int builtin_exit(char **args) {
    (void)args;
    disk_close();
    printf("Bye from oscdfs!\n");
    exit(0);
}

int builtin_dir(char **args) {
    uint32_t dir_inode;
    const char *path = args[1];

    if (path == NULL) {
        dir_inode = current_dir_inode;
    } else {
        uint32_t ino = find_inode_by_path(path, current_dir_inode);
        if (ino == (uint32_t)-1) {
            fprintf(stderr, "oscdfs: dir: path '%s' does not exist\n", path);
            return 1;
        }
        dir_inode = ino;
    }

    struct oscdfs_inode inode;
    if (read_inode(dir_inode, &inode) != 0) {
        fprintf(stderr, "oscdfs: dir: read inode %u failed\n", dir_inode);
        return 1;
    }
    if (!(inode.mode & OSCDFS_S_IFDIR)) {
        fprintf(stderr, "oscdfs: dir: '%s' is not a directory\n", path ? path : ".");
        return 1;
    }

    uint8_t *block_buf = malloc(OSCDFS_BLOCK_SIZE);
    if (!block_buf) {
        fprintf(stderr, "oscdfs: dir: malloc failed\n");
        return 1;
    }

    printf("%-8s %-28s %-8s %-6s %-4s\n", "inode", "name", "size", "mode", "uid");
    printf("----------------------------------------"
           "----------------------------------------\n");

    uint32_t logical_blk = 0;
    while (1) {
        int phys = inode_get_block(&inode, logical_blk, 0);
        if (phys < 0) break;

        if (read_block((uint32_t)phys, block_buf) != OSCDFS_BLOCK_SIZE) {
            free(block_buf);
            return 1;
        }

        struct oscdfs_dir_entry *entries = (struct oscdfs_dir_entry *)block_buf;
        for (int i = 0; i < OSCDFS_DIR_ENTRIES_PER_BLOCK; i++) {
            if (entries[i].inode_no == 0) continue;

            struct oscdfs_inode entry_inode;
            if (read_inode(entries[i].inode_no, &entry_inode) != 0) {
                free(block_buf);
                return 1;
            }
            printf("%-8u %-28s %-8u %06o %-4u\n",
                   entries[i].inode_no,
                   entries[i].name,
                   entry_inode.size,
                   entry_inode.mode & 07777,
                   entry_inode.uid);
        }
        logical_blk++;
    }
    fflush(stdout);
    free(block_buf);
    return 0;
}

int builtin_cd(char **args) {
    uint32_t target;

    if (args[1] == NULL) {
        /* 无参数切换到当前用户的家目录 */
        target = get_user_home_inode();
    } else {
        uint32_t ino = find_inode_by_path(args[1], current_dir_inode);
        if (ino == (uint32_t)-1) {
            fprintf(stderr, "oscdfs: cd: path '%s' does not exist\n", args[1]);
            return 1;
        }
        target = ino;
    }

    struct oscdfs_inode inode;
    if (read_inode(target, &inode) != 0) {
        fprintf(stderr, "oscdfs: cd: read inode failed\n");
        return 1;
    }
    if (!(inode.mode & OSCDFS_S_IFDIR)) {
        fprintf(stderr, "oscdfs: cd: not a directory\n");
        return 1;
    }

    current_dir_inode = target;
    return 0;
}

int builtin_create(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "用法: create <path>\n");
        return 1;
    }
    int new_ino = oscdfs_create(args[1]);
    if (new_ino < 0) return 1;
    printf("created '%s' (inode %d)\n", args[1], new_ino);
    return 0;
}

int builtin_open(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "用法: open <path> [r|w|rw]\n");
        return 1;
    }
    const char *path = args[1];
    int flags = OSCDFS_O_RDONLY;
    if (args[2] != NULL) {
        if (strcmp(args[2], "w") == 0)
            flags = OSCDFS_O_WRONLY;
        else if (strcmp(args[2], "rw") == 0)
            flags = OSCDFS_O_RDWR;
        else if (strcmp(args[2], "r") != 0) {
            fprintf(stderr, "oscdfs: open: unknown mode '%s'\n", args[2]);
            return 1;
        }
    }
    int fd = oscdfs_open(path, flags);
    if (fd < 0) return 1;
    printf("opened '%s' as fd %d\n", path, fd);
    return 0;
}

int builtin_close(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "用法: close <fd>\n");
        return 1;
    }
    int fd = atoi(args[1]);
    if (oscdfs_close(fd) != 0) return 1;
    printf("closed fd %d\n", fd);
    return 0;
}

int builtin_read(char **args) {
    if (args[1] == NULL || args[2] == NULL) {
        fprintf(stderr, "用法: read <fd> <n>\n");
        return 1;
    }
    int fd = atoi(args[1]);
    int n = atoi(args[2]);
    if (n <= 0) {
        fprintf(stderr, "oscdfs: read: invalid count\n");
        return 1;
    }
    char *buf = malloc(n + 1);
    if (!buf) {
        fprintf(stderr, "oscdfs: read: malloc failed\n");
        return 1;
    }
    int bytes = oscdfs_read(fd, buf, (uint32_t)n);
    if (bytes < 0) {
        free(buf);
        return 1;
    }
    if (bytes > 0) {
        buf[bytes] = '\0';
        printf("%s\n", buf);
    }
    printf("(%d bytes read)\n", bytes);
    fflush(stdout);
    free(buf);
    return 0;
}

int builtin_write(char **args) {
    if (args[1] == NULL || args[2] == NULL) {
        fprintf(stderr, "用法: write <fd> <data>\n");
        return 1;
    }
    int fd = atoi(args[1]);
    const char *data = args[2];
    int len = strlen(data);
    int bytes = oscdfs_write(fd, data, (uint32_t)len);
    if (bytes < 0) return 1;
    printf("(%d bytes written)\n", bytes);
    return 0;
}

int builtin_delete(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "用法: delete <path>\n");
        return 1;
    }
    if (oscdfs_delete(args[1]) != 0) return 1;
    printf("deleted '%s'\n", args[1]);
    return 0;
}

/* 登录命令：login <username> [password] */
int builtin_login(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "用法: login <username> [password]\n");
        return 1;
    }

    const char *username = args[1];
    const char *password = args[2] ? args[2] : "";   /* 无密码则为空字符串 */

    if (oscdfs_login(username, password) != 0)
        return 1;

    printf("logged in as '%s' (uid=%u, gid=%u)\n",
           username, current_uid, current_gid);
    return 0;
}

/* chmod 命令 */
int builtin_chmod(char **args) {
    if (args[1] == NULL || args[2] == NULL) {
        fprintf(stderr, "用法: chmod <path> <mode>\n");
        return 1;
    }

    uint32_t mode = (uint32_t)strtol(args[2], NULL, 8);
    if (oscdfs_chmod(args[1], mode) != 0)
        return 1;

    printf("chmod '%s' to %o\n", args[1], mode);
    return 0;
}

/* chown 命令 */
int builtin_chown(char **args) {
    if (args[1] == NULL || args[2] == NULL) {
        fprintf(stderr, "用法: chown <path> <uid> [gid]\n");
        return 1;
    }

    uint32_t new_uid = (uint32_t)strtoul(args[2], NULL, 10);
    uint32_t new_gid = (uint32_t)-1;
    if (args[3] != NULL)
        new_gid = (uint32_t)strtoul(args[3], NULL, 10);

    if (oscdfs_chown(args[1], new_uid, new_gid) != 0)
        return 1;

    printf("chown '%s' uid=%u", args[1], new_uid);
    if (new_gid != (uint32_t)-1)
        printf(" gid=%u", new_gid);
    printf("\n");
    return 0;
}