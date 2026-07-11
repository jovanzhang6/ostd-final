// oscddrv/test.c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#define OSCDDRV_IOC_MAGIC 'k'

#define CMD_GET_STATS   _IOR(OSCDDRV_IOC_MAGIC, 1, struct oscddrv_stats)
#define CMD_RESET       _IO(OSCDDRV_IOC_MAGIC, 2)
#define CMD_SET_BUFSIZE _IOW(OSCDDRV_IOC_MAGIC, 3, size_t)
#define CMD_SET_MODE    _IOW(OSCDDRV_IOC_MAGIC, 4, int)

#define MODE_NORMAL      0
#define MODE_PERFORMANCE 1

struct oscddrv_stats {
    size_t bufsize;
    int total_writes;
    int total_reads;
    int mode;
};

int main() {
    int fd = open("/dev/oscddrv", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct oscddrv_stats stats;
    int ret;

    // 1. 获取初始状态
    ret = ioctl(fd, CMD_GET_STATS, &stats);
    if (ret < 0) {
        perror("CMD_GET_STATS");
        goto out;
    }
    printf("Initial stats: bufsize=%zu, writes=%d, reads=%d, mode=%d\n",
           stats.bufsize, stats.total_writes, stats.total_reads, stats.mode);

    // 2. 测试 SET_BUFSIZE 扩大缓冲区
    size_t new_size = 8192;
    ret = ioctl(fd, CMD_SET_BUFSIZE, &new_size);
    if (ret < 0) {
        perror("CMD_SET_BUFSIZE");
        goto out;
    }
    printf("SET_BUFSIZE to 8192: success\n");

    // 再次获取状态验证
    ret = ioctl(fd, CMD_GET_STATS, &stats);
    if (ret < 0) {
        perror("CMD_GET_STATS after resize");
        goto out;
    }
    printf("After resize: bufsize=%zu\n", stats.bufsize);

    // 3. 测试 SET_MODE
    int mode = MODE_PERFORMANCE;
    ret = ioctl(fd, CMD_SET_MODE, &mode);
    if (ret < 0) {
        perror("CMD_SET_MODE");
        goto out;
    }
    printf("SET_MODE to performance: success\n");

    ret = ioctl(fd, CMD_GET_STATS, &stats);
    printf("After mode change: mode=%d\n", stats.mode);

    // 4. 写入一些数据以产生统计计数
    write(fd, "hello", 5);
    write(fd, "world", 5);

    ret = ioctl(fd, CMD_GET_STATS, &stats);
    printf("After writes: writes=%d, reads=%d\n", stats.total_writes, stats.total_reads);

    // 5. 读取数据（先重置偏移？需要新 fd 来读，因为当前 fd 偏移已到 10）
    // 这里我们仅验证统计，不重新读取数据。
    // 6. 测试 RESET
    ret = ioctl(fd, CMD_RESET);
    if (ret < 0) {
        perror("CMD_RESET");
        goto out;
    }
    printf("CMD_RESET: success\n");

    ret = ioctl(fd, CMD_GET_STATS, &stats);
    printf("After reset: writes=%d, reads=%d\n", stats.total_writes, stats.total_reads);

    // 测试无效命令
    ret = ioctl(fd, _IO(OSCDDRV_IOC_MAGIC, 99));
    if (ret < 0) perror("Invalid ioctl (expected)");

out:
    close(fd);
    return 0;
}