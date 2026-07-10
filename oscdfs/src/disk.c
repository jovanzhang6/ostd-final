// oscdfs/src/disk.c
#include "oscdfs.h"

static int disk_fd = -1;
static pthread_mutex_t disk_mutex;   /* 不再使用静态初始化，由 disk_open 动态初始化 */

/*
 * 打开磁盘文件并加锁
 * 成功返回0，失败返回-1
 */
int disk_open(const char *path)
{
    disk_fd = open(path, O_RDWR);
    if (disk_fd < 0) {
        perror("oscdfs: disk_open: open");
        return -1;
    }

    /* 获取文件级排他锁（非阻塞） */
    if (flock(disk_fd, LOCK_EX | LOCK_NB) < 0) {
        perror("oscdfs: disk_open: flock");
        close(disk_fd);
        disk_fd = -1;
        return -1;
    }

    /* 初始化线程互斥锁 */
    if (pthread_mutex_init(&disk_mutex, NULL) != 0) {
        perror("oscdfs: disk_open: pthread_mutex_init");
        flock(disk_fd, LOCK_UN);
        close(disk_fd);
        disk_fd = -1;
        return -1;
    }

    return 0;
}

/*
 * 关闭磁盘文件，释放锁
 */
void disk_close(void)
{
    if (disk_fd >= 0) {
        pthread_mutex_destroy(&disk_mutex);
        flock(disk_fd, LOCK_UN);
        close(disk_fd);
        disk_fd = -1;
    }
}

/*
 * 读取一个完整的块（4096字节）
 * 返回实际读取的字节数（应为4096），失败返回-1
 */
int read_block(uint32_t block_no, void *buf)
{
    off_t offset = (off_t)block_no * OSCDFS_BLOCK_SIZE;
    ssize_t total = 0;
    ssize_t n;

    if (disk_fd < 0 || buf == NULL) {
        fprintf(stderr, "oscdfs: read_block: invalid state\n");
        return -1;
    }

    pthread_mutex_lock(&disk_mutex);

    if (lseek(disk_fd, offset, SEEK_SET) < 0) {
        perror("oscdfs: read_block: lseek");
        pthread_mutex_unlock(&disk_mutex);
        return -1;
    }

    while (total < OSCDFS_BLOCK_SIZE) {
        n = read(disk_fd, (char *)buf + total, OSCDFS_BLOCK_SIZE - total);
        if (n < 0) {
            perror("oscdfs: read_block: read");
            pthread_mutex_unlock(&disk_mutex);
            return -1;
        }
        if (n == 0) {
            fprintf(stderr, "oscdfs: read_block: unexpected EOF at block %u\n", block_no);
            pthread_mutex_unlock(&disk_mutex);
            return -1;
        }
        total += n;
    }

    pthread_mutex_unlock(&disk_mutex);
    return (int)total;
}

/*
 * 写入一个完整的块（4096字节）
 * 返回实际写入的字节数（应为4096），失败返回-1
 */
int write_block(uint32_t block_no, const void *buf)
{
    off_t offset = (off_t)block_no * OSCDFS_BLOCK_SIZE;
    ssize_t total = 0;
    ssize_t n;

    if (disk_fd < 0 || buf == NULL) {
        fprintf(stderr, "oscdfs: write_block: invalid state\n");
        return -1;
    }

    pthread_mutex_lock(&disk_mutex);

    if (lseek(disk_fd, offset, SEEK_SET) < 0) {
        perror("oscdfs: write_block: lseek");
        pthread_mutex_unlock(&disk_mutex);
        return -1;
    }

    while (total < OSCDFS_BLOCK_SIZE) {
        n = write(disk_fd, (const char *)buf + total, OSCDFS_BLOCK_SIZE - total);
        if (n < 0) {
            perror("oscdfs: write_block: write");
            pthread_mutex_unlock(&disk_mutex);
            return -1;
        }
        if (n == 0) {
            fprintf(stderr, "oscdfs: write_block: write returned 0 at block %u\n", block_no);
            pthread_mutex_unlock(&disk_mutex);
            return -1;
        }
        total += n;
    }

    pthread_mutex_unlock(&disk_mutex);
    return (int)total;
}