// oscdk/test.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SYS_PROC_COLLECT  548
#define SYS_PROC_SNAPSHOT 549
#define SYS_PROC_STAT     550

struct proc_info {
    int pid;
    char comm[16];
    long state;
    unsigned long utime;
    unsigned long stime;
    unsigned long vsize;
};

struct proc_stat {
    int total;
    int running;
    int sleeping;
    int uninterruptible;
    int zombie;
    int stopped;
    int other;
};

int main() {
    long ret;
    int i;

    /* 1. 测试 sys_proc_collect */
    struct proc_info info[128];
    ret = syscall(SYS_PROC_COLLECT, info, 128);
    if (ret > 0) {
        printf("proc_collect: 收集到 %ld 个进程，显示前5个:\n", ret);
        for (i = 0; i < ret && i < 5; i++) {
            printf("  PID: %-6d comm: %-16s state: %ld\n",
                   info[i].pid, info[i].comm, info[i].state);
        }
    } else {
        printf("proc_collect 调用失败，返回值: %ld\n", ret);
    }

    /* 2. 测试 sys_proc_stat */
    struct proc_stat stat;
    ret = syscall(SYS_PROC_STAT, &stat);
    if (ret == 0) {
        printf("\nproc_stat:\n");
        printf("  总进程数: %d\n", stat.total);
        printf("  运行中  : %d\n", stat.running);
        printf("  睡眠    : %d\n", stat.sleeping);
        printf("  不可中断: %d\n", stat.uninterruptible);
        printf("  僵尸    : %d\n", stat.zombie);
        printf("  暂停    : %d\n", stat.stopped);
        printf("  其他    : %d\n", stat.other);
    } else {
        printf("proc_stat 调用失败，返回值: %ld\n", ret);
    }

    /* 3. 测试 sys_proc_snapshot */
    char buf[8192];
    ret = syscall(SYS_PROC_SNAPSHOT, buf, sizeof(buf));
    if (ret > 0) {
        printf("\nproc_snapshot (%ld bytes):\n%s\n", ret, buf);
    } else if (ret == -90) { /* EMSGSIZE = 90 */
        printf("\nproc_snapshot: 缓冲区不足，部分内容:\n%s\n", buf);
    } else {
        printf("proc_snapshot 调用失败，返回值: %ld\n", ret);
    }

    return 0;
}