// oscdfs/src/mkfs.c
#include "oscdfs.h"
#include <sys/stat.h>

int mkfs_disk(const char *path)
{
    int fd;
    struct stat st;

    /* 若文件已存在则直接覆盖 */
    if (stat(path, &st) == 0) {
        printf("oscdfs: mkfs: '%s' already exists, overwriting...\n", path);
    }

    /* 创建或截断文件 */
    fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("oscdfs: mkfs: open");
        return -1;
    }

    /* 设定文件大小为 8MB */
    if (ftruncate(fd, OSCDFS_TOTAL_BLOCKS * OSCDFS_BLOCK_SIZE) < 0) {
        perror("oscdfs: mkfs: ftruncate");
        close(fd);
        unlink(path);
        return -1;
    }
    close(fd);

    /* 打开磁盘文件（获取文件锁和互斥锁） */
    if (disk_open(path) != 0) {
        unlink(path);
        return -1;
    }

    /* 1. 初始化超级块 */
    super_init();

    /* 2. 初始化块位图 */
    block_bitmap_init();

    /* 3. 初始化 inode 位图（全部空闲） */
    inode_bitmap_init();

    /* 4. 分配根目录 inode（预期为 1） */
    int root_ino = alloc_inode();
    if (root_ino != 1) {
        fprintf(stderr, "oscdfs: mkfs: unexpected root inode %d (expected 1)\n", root_ino);
        disk_close();
        unlink(path);
        return -1;
    }

    /* 5. 初始化根 inode */
    inode_init((uint32_t)root_ino, OSCDFS_DEFAULT_DIR_MODE, 0, 0);

    /* 6. 初始化根目录内容 */
    if (dir_init_root((uint32_t)root_ino) != 0) {
        fprintf(stderr, "oscdfs: mkfs: failed to init root directory\n");
        disk_close();
        unlink(path);
        return -1;
    }

    disk_close();
    printf("oscdfs: mkfs: '%s' created successfully (8MB, %u blocks)\n",
           path, OSCDFS_TOTAL_BLOCKS);

    return 0;
}