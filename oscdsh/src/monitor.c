// oscdsh/src/monitor.c
#define _GNU_SOURCE
#include "oscdsh.h"
#include <sys/statvfs.h>
#include <time.h>

/* ANSI 颜色定义 */
#define M_COLOR_HEAD "\033[1;36m"
#define M_COLOR_TITLE "\033[1;33m"
#define M_COLOR_LABEL "\033[1;32m"
#define M_COLOR_RESET "\033[0m"

/* 用于进程信息收集的结构体 */
typedef struct {
    char name[256];
    char state[64];
    int pid;
    int ppid;
    long vm_rss_kb;
    int threads;
} ProcInfo;

/* 调度表（MonitorCommand 定义在 monitor.h） */

/* 前向声明（非 static，与 monitor.h 一致） */
int monitor_overview(char **args);
int monitor_process(char **args);
int monitor_memory(char **args);
int monitor_network(char **args);
int monitor_filesystem(char **args);
int monitor_device(char **args);
int monitor_power(char **args);
int monitor_save(char **args);

MonitorCommand monitor_commands[] = {
    {"overview",   monitor_overview,   "显示系统概览 (CPU/内存/负载)"},
    {"process",    monitor_process,    "显示进程列表"},
    {"memory",     monitor_memory,     "显示详细内存信息"},
    {"network",    monitor_network,    "显示网络流量统计"},
    {"filesystem", monitor_filesystem, "显示磁盘I/O和挂载信息"},
    {"device",     monitor_device,     "显示设备列表"},
    {"power",      monitor_power,      "显示CPU频率和电源信息"},
    {"save",       monitor_save,       "导出监控数据到文件"},
    {NULL, NULL, NULL}
};

/* ---- 辅助函数 ---- */

/* 将字节数格式化为人类可读字符串 */
static void format_bytes(char *buf, size_t buf_size, unsigned long long bytes) {
    if (bytes >= 1073741824ULL)
        snprintf(buf, buf_size, "%.2f GB", (double)bytes / 1073741824.0);
    else if (bytes >= 1048576ULL)
        snprintf(buf, buf_size, "%.2f MB", (double)bytes / 1048576.0);
    else if (bytes >= 1024ULL)
        snprintf(buf, buf_size, "%.2f KB", (double)bytes / 1024.0);
    else
        snprintf(buf, buf_size, "%llu B", bytes);
}

/* 打印分隔线 */
static void print_separator(void) {
    printf("%s----------------------------------------%s\n", M_COLOR_HEAD, M_COLOR_RESET);
}

/* ---- monitor_overview ---- */
int monitor_overview(char **args) {
    (void)args;
    FILE *fp;
    char line[256];
    unsigned long long uptime_secs;

    print_separator();
    printf("%s            OSCD 系统监控概览%s\n", M_COLOR_TITLE, M_COLOR_RESET);
    print_separator();

    /* --- CPU 使用率 --- */
    fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("monitor: fopen /proc/stat");
        return 1;
    }
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        fprintf(stderr, "monitor: 读取 /proc/stat 失败\n");
        return 1;
    }
    fclose(fp);

    unsigned long cpu_user, cpu_nice, cpu_sys, cpu_idle;
    unsigned long cpu_iowait, cpu_irq, cpu_softirq, cpu_steal;
    if (sscanf(line, "cpu  %lu %lu %lu %lu %lu %lu %lu %lu",
               &cpu_user, &cpu_nice, &cpu_sys, &cpu_idle,
               &cpu_iowait, &cpu_irq, &cpu_softirq, &cpu_steal) < 4) {
        fprintf(stderr, "monitor: 解析 /proc/stat 失败\n");
        return 1;
    }

    unsigned long total = cpu_user + cpu_nice + cpu_sys + cpu_idle
                        + cpu_iowait + cpu_irq + cpu_softirq + cpu_steal;
    unsigned long idle_total = cpu_idle + cpu_iowait;
    double cpu_usage = (total > 0) ? 100.0 * (total - idle_total) / total : 0.0;

    printf("%sCPU 使用率:%s  %.1f%%\n", M_COLOR_LABEL, M_COLOR_RESET, cpu_usage);
    printf("  user=%lu  nice=%lu  system=%lu  idle=%lu  iowait=%lu\n",
           cpu_user, cpu_nice, cpu_sys, cpu_idle, cpu_iowait);
    printf("  irq=%lu  softirq=%lu  steal=%lu\n",
           cpu_irq, cpu_softirq, cpu_steal);

    /* --- 内存信息 --- */
    fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("monitor: fopen /proc/meminfo");
        return 1;
    }

    long mem_total = 0, mem_free = 0, mem_avail = 0;
    long mem_buffers = 0, mem_cached = 0;
    long swap_total = 0, swap_free = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %ld kB", &mem_total) == 1) continue;
        if (sscanf(line, "MemFree: %ld kB", &mem_free) == 1) continue;
        if (sscanf(line, "MemAvailable: %ld kB", &mem_avail) == 1) continue;
        if (sscanf(line, "Buffers: %ld kB", &mem_buffers) == 1) continue;
        if (sscanf(line, "Cached: %ld kB", &mem_cached) == 1) continue;
        if (sscanf(line, "SwapTotal: %ld kB", &swap_total) == 1) continue;
        if (sscanf(line, "SwapFree: %ld kB", &swap_free) == 1) continue;
    }
    fclose(fp);

    long mem_used = mem_total - mem_free - mem_buffers - mem_cached;
    long swap_used = swap_total - swap_free;

    printf("\n%s内存信息:%s\n", M_COLOR_LABEL, M_COLOR_RESET);
    printf("  总计: %ld MB    已用: %ld MB    空闲: %ld MB\n",
           mem_total / 1024, mem_used / 1024, mem_free / 1024);
    printf("  可用: %ld MB    Buffers: %ld MB    Cached: %ld MB\n",
           mem_avail / 1024, mem_buffers / 1024, mem_cached / 1024);
    if (swap_total > 0) {
        printf("  Swap: 总计 %ld MB, 已用 %ld MB, 空闲 %ld MB\n",
               swap_total / 1024, swap_used / 1024, swap_free / 1024);
    } else {
        printf("  Swap: 未启用\n");
    }

    /* --- 系统负载 --- */
    fp = fopen("/proc/loadavg", "r");
    if (!fp) {
        perror("monitor: fopen /proc/loadavg");
        return 1;
    }
    double load1, load5, load15;
    if (fscanf(fp, "%lf %lf %lf", &load1, &load5, &load15) != 3) {
        fclose(fp);
        fprintf(stderr, "monitor: 解析 /proc/loadavg 失败\n");
        return 1;
    }
    fclose(fp);

    printf("\n%s系统负载:%s  %.2f (1min)  %.2f (5min)  %.2f (15min)\n",
           M_COLOR_LABEL, M_COLOR_RESET, load1, load5, load15);

    /* --- 运行时间 --- */
    {
        FILE *ufp = fopen("/proc/uptime", "r");
        if (ufp) {
            if (fscanf(ufp, "%llu", &uptime_secs) == 1) {
                unsigned long days = uptime_secs / 86400;
                unsigned long hours = (uptime_secs % 86400) / 3600;
                unsigned long mins = (uptime_secs % 3600) / 60;
                unsigned long secs = uptime_secs % 60;
                printf("\n%s运行时间:%s  %lu天 %02lu:%02lu:%02lu\n",
                       M_COLOR_LABEL, M_COLOR_RESET, days, hours, mins, secs);
            }
            fclose(ufp);
        }
    }

    print_separator();
    return 0;
}

/* ---- monitor_process 比较函数 ---- */
static int cmp_proc_by_rss(const void *a, const void *b) {
    const ProcInfo *pa = (const ProcInfo *)a;
    const ProcInfo *pb = (const ProcInfo *)b;
    if (pa->vm_rss_kb < pb->vm_rss_kb) return 1;
    if (pa->vm_rss_kb > pb->vm_rss_kb) return -1;
    return 0;
}

int monitor_process(char **args) {
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("monitor: opendir /proc");
        return 1;
    }

    ProcInfo procs[4096];
    int count = 0;
    const char *filter_name = NULL;
    int sort_by_rss = 0;

    /* 解析参数（跳过 args[0]="monitor" 和 args[1]="process"） */
    for (int i = 2; args[i] != NULL; i++) {
        if (strcmp(args[i], "--sort-rss") == 0) {
            sort_by_rss = 1;
        } else if (args[i][0] != '-') {
            filter_name = args[i];
        }
    }

    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL && count < 4096) {
        /* 只处理数值型目录（PID） */
        int is_num = 1;
        for (char *p = entry->d_name; *p; p++) {
            if (!isdigit((unsigned char)*p)) { is_num = 0; break; }
        }
        if (!is_num) continue;

        char path[512];
        snprintf(path, sizeof(path), "/proc/%s/status", entry->d_name);

        FILE *sfp = fopen(path, "r");
        if (!sfp) continue;

        ProcInfo pi;
        memset(&pi, 0, sizeof(pi));
        pi.pid = atoi(entry->d_name);
        pi.vm_rss_kb = -1;
        pi.threads = -1;

        char line[256];
        while (fgets(line, sizeof(line), sfp)) {
            if (strncmp(line, "Name:", 5) == 0) {
                sscanf(line + 5, " %255s", pi.name);
            } else if (strncmp(line, "State:", 6) == 0) {
                sscanf(line + 6, " %63s", pi.state);
            } else if (strncmp(line, "Pid:", 4) == 0) {
                sscanf(line + 4, " %d", &pi.ppid);
            } else if (strncmp(line, "PPid:", 5) == 0) {
                sscanf(line + 5, " %d", &pi.ppid);
            } else if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line + 6, " %ld", &pi.vm_rss_kb);
            } else if (strncmp(line, "Threads:", 8) == 0) {
                sscanf(line + 8, " %d", &pi.threads);
            }
        }
        fclose(sfp);

        if (pi.name[0] == '\0') continue;

        /* 名称过滤 */
        if (filter_name && strcasestr(pi.name, filter_name) == NULL) {
            /* strcasestr 是 POSIX 扩展，Linux 可用 */
            continue;
        }

        procs[count++] = pi;
    }
    closedir(proc_dir);

    if (count == 0) {
        if (filter_name)
            printf("monitor: 未找到匹配的进程 (%s)\n", filter_name);
        else
            printf("monitor: 未找到进程\n");
        return 1;
    }

    /* 排序 */
    if (sort_by_rss) {
        qsort(procs, count, sizeof(ProcInfo), cmp_proc_by_rss);
    }

    /* 打印表头 */
    print_separator();
    printf("%s              OSCD 进程列表%s\n", M_COLOR_TITLE, M_COLOR_RESET);
    print_separator();
    printf("%s%-6s %-20s %-10s %5s %5s %8s %s\n",
           M_COLOR_HEAD,
           "PID", "NAME", "STATE", "PPID", "THR", "RSS(kB)",
           M_COLOR_RESET);
    print_separator();

    for (int i = 0; i < count; i++) {
        printf("%-6d %-20.20s %-10s %5d %5d %8ld\n",
               procs[i].pid,
               procs[i].name,
               procs[i].state,
               procs[i].ppid,
               procs[i].threads >= 0 ? procs[i].threads : 0,
               procs[i].vm_rss_kb >= 0 ? procs[i].vm_rss_kb : 0);
    }

    printf("\n%s总计:%s %d 个进程\n", M_COLOR_LABEL, M_COLOR_RESET, count);
    print_separator();
    return 0;
}

/* ---- monitor_memory ---- */
int monitor_memory(char **args) {
    (void)args;
    FILE *fp;
    char line[256];

    print_separator();
    printf("%s            OSCD 详细内存信息%s\n", M_COLOR_TITLE, M_COLOR_RESET);
    print_separator();

    /* /proc/meminfo 详细信息 */
    fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("monitor: fopen /proc/meminfo");
        return 1;
    }

    printf("%s/proc/meminfo:%s\n", M_COLOR_LABEL, M_COLOR_RESET);
    int printed = 0;
    while (fgets(line, sizeof(line), fp)) {
        /* 仅打印关键行 */
        if (strncmp(line, "MemTotal:", 9) == 0 ||
            strncmp(line, "MemFree:", 8) == 0 ||
            strncmp(line, "MemAvailable:", 13) == 0 ||
            strncmp(line, "Buffers:", 8) == 0 ||
            strncmp(line, "Cached:", 7) == 0 ||
            strncmp(line, "SwapCached:", 11) == 0 ||
            strncmp(line, "Active:", 7) == 0 ||
            strncmp(line, "Inactive:", 9) == 0 ||
            strncmp(line, "Active(anon):", 13) == 0 ||
            strncmp(line, "Inactive(anon):", 15) == 0 ||
            strncmp(line, "Active(file):", 13) == 0 ||
            strncmp(line, "Inactive(file):", 15) == 0 ||
            strncmp(line, "Unevictable:", 12) == 0 ||
            strncmp(line, "Mlocked:", 8) == 0 ||
            strncmp(line, "SwapTotal:", 10) == 0 ||
            strncmp(line, "SwapFree:", 9) == 0 ||
            strncmp(line, "Dirty:", 6) == 0 ||
            strncmp(line, "Writeback:", 10) == 0 ||
            strncmp(line, "AnonPages:", 10) == 0 ||
            strncmp(line, "Mapped:", 7) == 0 ||
            strncmp(line, "Shmem:", 6) == 0 ||
            strncmp(line, "Slab:", 5) == 0 ||
            strncmp(line, "SReclaimable:", 13) == 0 ||
            strncmp(line, "SUnreclaim:", 11) == 0 ||
            strncmp(line, "KernelStack:", 12) == 0 ||
            strncmp(line, "PageTables:", 11) == 0 ||
            strncmp(line, "NFS_Unstable:", 13) == 0 ||
            strncmp(line, "Bounce:", 7) == 0 ||
            strncmp(line, "WritebackTmp:", 13) == 0 ||
            strncmp(line, "CommitLimit:", 12) == 0 ||
            strncmp(line, "Committed_AS:", 13) == 0 ||
            strncmp(line, "VmallocTotal:", 13) == 0 ||
            strncmp(line, "VmallocUsed:", 12) == 0 ||
            strncmp(line, "VmallocChunk:", 13) == 0 ||
            strncmp(line, "HardwareCorrupted:", 18) == 0 ||
            strncmp(line, "AnonHugePages:", 14) == 0 ||
            strncmp(line, "HugePages_Total:", 16) == 0 ||
            strncmp(line, "HugePages_Free:", 15) == 0 ||
            strncmp(line, "HugePages_Rsvd:", 15) == 0 ||
            strncmp(line, "HugePages_Surp:", 15) == 0 ||
            strncmp(line, "Hugepagesize:", 13) == 0 ||
            strncmp(line, "DirectMap4k:", 12) == 0 ||
            strncmp(line, "DirectMap2M:", 12) == 0 ||
            strncmp(line, "DirectMap1G:", 12) == 0) {
            /* 去掉末尾换行 */
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
            printf("  %s\n", line);
            printed++;
        }
    }
    fclose(fp);

    if (printed == 0) {
        printf("  (无数据)\n");
    }

    /* /proc/slabinfo - SLUB 缓存信息 */
    printf("\n%s/proc/slabinfo (SLUB 缓存):%s\n", M_COLOR_LABEL, M_COLOR_RESET);
    fp = fopen("/proc/slabinfo", "r");
    if (!fp) {
        printf("  无法读取 /proc/slabinfo (需要 root 权限)\n");
    } else {
        int line_count = 0;
        /* 跳过标题行 */
        if (fgets(line, sizeof(line), fp)) {
            /* 第一行可能是版本信息，跳过 */
        }
        if (fgets(line, sizeof(line), fp)) {
            /* 第二行是列标题 */
        }

        while (fgets(line, sizeof(line), fp) && line_count < 30) {
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
            printf("  %s\n", line);
            line_count++;
        }

        if (!feof(fp)) {
            printf("  ... (还有更多条目, 仅显示前 30 条)\n");
        }
        fclose(fp);
    }

    print_separator();
    return 0;
}

/* ---- monitor_network ---- */
int monitor_network(char **args) {
    (void)args;
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        perror("monitor: fopen /proc/net/dev");
        return 1;
    }

    char line[256];

    print_separator();
    printf("%s            OSCD 网络流量统计%s\n", M_COLOR_TITLE, M_COLOR_RESET);
    print_separator();

    /* 跳过两行标题 */
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return 1; }
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return 1; }

    int if_count = 0;
    while (fgets(line, sizeof(line), fp)) {
        char ifname[64];
        unsigned long long rx_bytes, rx_packets, rx_errs, rx_drop;
        unsigned long long tx_bytes, tx_packets, tx_errs, tx_drop;

        int n = sscanf(line, "%63[^:]: %llu %llu %llu %llu %*u %*u %*u %*u "
                             "%llu %llu %llu %llu %*u %*u %*u %*u",
                       ifname,
                       &rx_bytes, &rx_packets, &rx_errs, &rx_drop,
                       &tx_bytes, &tx_packets, &tx_errs, &tx_drop);

        if (n < 9) continue;

        /* 排除 lo 接口 */
        if (strcmp(ifname, "lo") == 0) continue;

        char rx_str[32], tx_str[32];
        format_bytes(rx_str, sizeof(rx_str), rx_bytes);
        format_bytes(tx_str, sizeof(tx_str), tx_bytes);

        if_count++;
        printf("\n%s  接口: %s%s\n", M_COLOR_LABEL, ifname, M_COLOR_RESET);
        printf("    接收: %s (%llu 包, %llu 错误, %llu 丢弃)\n",
               rx_str, rx_packets, rx_errs, rx_drop);
        printf("    发送: %s (%llu 包, %llu 错误, %llu 丢弃)\n",
               tx_str, tx_packets, tx_errs, tx_drop);
    }
    fclose(fp);

    if (if_count == 0) {
        printf("  未找到非回环接口\n");
    }

    print_separator();
    return 0;
}

/* ---- monitor_filesystem ---- */
int monitor_filesystem(char **args) {
    (void)args;
    FILE *fp;
    char line[512];

    print_separator();
    printf("%s            OSCD 磁盘I/O与挂载信息%s\n", M_COLOR_TITLE, M_COLOR_RESET);
    print_separator();

    /* /proc/diskstats - 磁盘 I/O */
    printf("%s磁盘 I/O 统计 (/proc/diskstats):%s\n", M_COLOR_LABEL, M_COLOR_RESET);
    fp = fopen("/proc/diskstats", "r");
    if (!fp) {
        perror("monitor: fopen /proc/diskstats");
    } else {
        int dev_count = 0;
        while (fgets(line, sizeof(line), fp)) {
            int major, minor;
            char dev_name[64];
            unsigned long long rd_ios, rd_merges, rd_sectors, rd_ticks;
            unsigned long long wr_ios, wr_merges, wr_sectors, wr_ticks;
            unsigned long long ios_in_prog, tot_ticks, rq_ticks;

            int n = sscanf(line, "%d %d %63s %llu %llu %llu %llu "
                                 "%llu %llu %llu %llu %llu %llu %llu",
                           &major, &minor, dev_name,
                           &rd_ios, &rd_merges, &rd_sectors, &rd_ticks,
                           &wr_ios, &wr_merges, &wr_sectors, &wr_ticks,
                           &ios_in_prog, &tot_ticks, &rq_ticks);
            if (n < 14) continue;

            /* 只显示顶层设备（不含分区名中的数字结尾，如 sda 而非 sda1）
               但简化处理：只要不是分区就显示，分区通常含有数字结尾 */
            size_t dnlen = strlen(dev_name);
            int is_partition = 0;
            if (dnlen > 0 && dev_name[dnlen - 1] >= '0' && dev_name[dnlen - 1] <= '9')
                is_partition = 1;
            /* 跳过 ram, loop, zram 等虚拟设备 */
            if (strncmp(dev_name, "ram", 3) == 0) continue;
            if (strncmp(dev_name, "loop", 4) == 0) continue;
            if (strncmp(dev_name, "zram", 4) == 0) continue;

            /* 仅显示主设备（不显示分区）以减少输出量 */
            if (is_partition) continue;

            char rd_str[32], wr_str[32];
            format_bytes(rd_str, sizeof(rd_str), rd_sectors * 512ULL);
            format_bytes(wr_str, sizeof(wr_str), wr_sectors * 512ULL);

            printf("  %-8s 读: %s (%llu 次)  写: %s (%llu 次)  IO进行中: %llu\n",
                   dev_name, rd_str, rd_ios, wr_str, wr_ios, ios_in_prog);
            dev_count++;
        }
        fclose(fp);

        if (dev_count == 0) {
            printf("  (未找到磁盘设备)\n");
        }
    }

    /* /proc/mounts - 挂载点 + statvfs */
    printf("\n%s挂载点与文件系统使用情况:%s\n", M_COLOR_LABEL, M_COLOR_RESET);
    fp = fopen("/proc/mounts", "r");
    if (!fp) {
        perror("monitor: fopen /proc/mounts");
        return 1;
    }

    int mnt_count = 0;
    while (fgets(line, sizeof(line), fp) && mnt_count < 40) {
        char mnt_dev[256], mnt_point[256], mnt_fstype[64], mnt_opts[256];
        int dump, pass;

        int n = sscanf(line, "%255s %255s %63s %255s %d %d",
                       mnt_dev, mnt_point, mnt_fstype, mnt_opts, &dump, &pass);
        if (n < 6) continue;

        /* 跳过 cgroup, sysfs, proc, devpts, tmpfs 等伪文件系统 */
        if (strcmp(mnt_fstype, "proc") == 0 ||
            strcmp(mnt_fstype, "sysfs") == 0 ||
            strcmp(mnt_fstype, "cgroup") == 0 ||
            strcmp(mnt_fstype, "cgroup2") == 0 ||
            strcmp(mnt_fstype, "devpts") == 0 ||
            strcmp(mnt_fstype, "devtmpfs") == 0 ||
            strcmp(mnt_fstype, "pstore") == 0 ||
            strcmp(mnt_fstype, "securityfs") == 0 ||
            strcmp(mnt_fstype, "autofs") == 0 ||
            strcmp(mnt_fstype, "debugfs") == 0 ||
            strcmp(mnt_fstype, "tracefs") == 0 ||
            strcmp(mnt_fstype, "configfs") == 0 ||
            strcmp(mnt_fstype, "efivarfs") == 0 ||
            strcmp(mnt_fstype, "mqueue") == 0 ||
            strcmp(mnt_fstype, "hugetlbfs") == 0 ||
            strcmp(mnt_fstype, "bpf") == 0 ||
            strncmp(mnt_dev, "cgroup", 6) == 0 ||
            strncmp(mnt_fstype, "fuse.", 5) == 0 ||
            strcmp(mnt_fstype, "overlay") == 0)
            continue;

        /* 尝试用 statvfs 获取使用情况 */
        struct statvfs stat;
        unsigned long long total_size = 0, free_size = 0, used_size = 0;
        double used_pct = 0.0;
        int vfs_ok = 0;

        if (statvfs(mnt_point, &stat) == 0 && stat.f_frsize > 0) {
            total_size = (unsigned long long)stat.f_blocks * stat.f_frsize;
            free_size  = (unsigned long long)stat.f_bfree  * stat.f_frsize;
            used_size  = total_size - free_size;
            if (total_size > 0)
                used_pct = 100.0 * used_size / total_size;
            vfs_ok = 1;
        }

        char total_str[32], used_str[32], free_str[32];
        if (vfs_ok) {
            format_bytes(total_str, sizeof(total_str), total_size);
            format_bytes(used_str, sizeof(used_str), used_size);
            format_bytes(free_str, sizeof(free_str), free_size);
            printf("  %-20s %-10s 总计 %-8s 已用 %-8s 空闲 %-8s (%.1f%%)\n",
                   mnt_point, mnt_fstype, total_str, used_str, free_str, used_pct);
        } else {
            printf("  %-20s %-10s\n", mnt_point, mnt_fstype);
        }
        mnt_count++;
    }
    fclose(fp);

    print_separator();
    return 0;
}

/* ---- monitor_device ---- */
int monitor_device(char **args) {
    (void)args;
    FILE *fp;
    char line[256];

    print_separator();
    printf("%s            OSCD 设备列表%s\n", M_COLOR_TITLE, M_COLOR_RESET);
    print_separator();

    /* /proc/devices */
    fp = fopen("/proc/devices", "r");
    if (!fp) {
        perror("monitor: fopen /proc/devices");
        return 1;
    }

    int section = 0; /* 0=开头, 1=字符设备, 2=块设备 */
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        if (strcmp(line, "Character devices:") == 0) {
            section = 1;
            printf("\n%s字符设备:%s\n", M_COLOR_LABEL, M_COLOR_RESET);
            continue;
        }
        if (strcmp(line, "Block devices:") == 0) {
            section = 2;
            printf("\n%s块设备:%s\n", M_COLOR_LABEL, M_COLOR_RESET);
            continue;
        }

        if (section > 0 && strlen(line) > 0) {
            printf("  %s\n", line);
        }
    }
    fclose(fp);

    /* /sys/class/ - 设备分类 */
    printf("\n%s/sys/class/ 设备类别:%s\n", M_COLOR_LABEL, M_COLOR_RESET);
    DIR *class_dir = opendir("/sys/class");
    if (!class_dir) {
        perror("monitor: opendir /sys/class");
    } else {
        struct dirent *entry;
        int class_count = 0;
        while ((entry = readdir(class_dir)) != NULL && class_count < 50) {
            if (entry->d_name[0] == '.') continue;
            if (class_count > 0 && class_count % 8 == 0)
                printf("\n");
            printf("  %-16s", entry->d_name);
            class_count++;
        }
        closedir(class_dir);
        if (class_count > 0) printf("\n");
        printf("  (共 %d 个设备类别)\n", class_count);
    }

    print_separator();
    return 0;
}

/* ---- monitor_power ---- */
int monitor_power(char **args) {
    (void)args;
    char path[512];
    char line[64];
    int cpu_count = 0;

    print_separator();
    printf("%s            OSCD CPU频率与电源信息%s\n", M_COLOR_TITLE, M_COLOR_RESET);
    print_separator();

    for (int i = 0; i < 1024; i++) {
        char dir[64];
        snprintf(dir, sizeof(dir), "/sys/devices/system/cpu/cpu%d", i);

        /* 检查 CPU 目录是否存在 */
        snprintf(path, sizeof(path), "%s/cpufreq/scaling_cur_freq", dir);
        FILE *fp = fopen(path, "r");
        if (!fp) {
            /* 检查该 CPU 是否 online */
            snprintf(path, sizeof(path), "%s/online", dir);
            fp = fopen(path, "r");
            if (fp) {
                int online;
                if (fscanf(fp, "%d", &online) == 1 && online == 0) {
                    fclose(fp);
                    continue; /* CPU 不在线 */
                }
                fclose(fp);
            }
            continue; /* 没有 cpufreq 或 CPU 不存在 */
        }
        fclose(fp);

        cpu_count++;
        printf("\n%sCPU %d:%s\n", M_COLOR_LABEL, i, M_COLOR_RESET);

        /* 当前频率 */
        snprintf(path, sizeof(path), "%s/cpufreq/scaling_cur_freq", dir);
        fp = fopen(path, "r");
        if (fp) {
            if (fscanf(fp, "%63s", line) == 1) {
                unsigned long freq_khz = strtoul(line, NULL, 10);
                printf("  当前频率: %lu MHz\n", freq_khz / 1000);
            }
            fclose(fp);
        }

        /* 最小频率 */
        snprintf(path, sizeof(path), "%s/cpufreq/scaling_min_freq", dir);
        fp = fopen(path, "r");
        if (fp) {
            if (fscanf(fp, "%63s", line) == 1) {
                unsigned long freq_khz = strtoul(line, NULL, 10);
                printf("  最小频率: %lu MHz\n", freq_khz / 1000);
            }
            fclose(fp);
        }

        /* 最大频率 */
        snprintf(path, sizeof(path), "%s/cpufreq/scaling_max_freq", dir);
        fp = fopen(path, "r");
        if (fp) {
            if (fscanf(fp, "%63s", line) == 1) {
                unsigned long freq_khz = strtoul(line, NULL, 10);
                printf("  最大频率: %lu MHz\n", freq_khz / 1000);
            }
            fclose(fp);
        }

        /* 调速器 */
        snprintf(path, sizeof(path), "%s/cpufreq/scaling_governor", dir);
        fp = fopen(path, "r");
        if (fp) {
            if (fscanf(fp, "%63s", line) == 1) {
                printf("  调速器: %s\n", line);
            }
            fclose(fp);
        }
    }

    if (cpu_count == 0) {
        printf("\n  无法读取 CPU 频率信息（需要 root 权限或 cpufreq 驱动未加载）\n");
    } else {
        printf("\n%s共读取 %d 个 CPU 核心的频率信息%s\n", M_COLOR_LABEL, cpu_count, M_COLOR_RESET);
    }

    print_separator();
    return 0;
}

/* ---- monitor_save ---- */
int monitor_save(char **args) {
    if (args[2] == NULL) {
        fprintf(stderr, "monitor: save 需要文件名参数\n");
        fprintf(stderr, "用法: monitor save <文件名>\n");
        return 1;
    }

    FILE *out = fopen(args[2], "w");
    if (!out) {
        perror("monitor: fopen 输出文件");
        return 1;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(out, "========================================\n");
    fprintf(out, " OSCD 系统监控报告\n");
    fprintf(out, " 生成时间: %s\n", time_buf);
    fprintf(out, "========================================\n\n");

    /* --- CPU --- */
    {
        FILE *fp = fopen("/proc/stat", "r");
        if (fp) {
            char line[256];
            if (fgets(line, sizeof(line), fp)) {
                unsigned long user, nice, sys, idle, iowait, irq, softirq, steal;
                sscanf(line, "cpu  %lu %lu %lu %lu %lu %lu %lu %lu",
                       &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
                unsigned long total = user + nice + sys + idle + iowait + irq + softirq + steal;
                unsigned long idle_total = idle + iowait;
                double usage = (total > 0) ? 100.0 * (total - idle_total) / total : 0.0;
                fprintf(out, "CPU 使用率: %.1f%%\n", usage);
                fprintf(out, "  user=%lu nice=%lu system=%lu idle=%lu iowait=%lu "
                             "irq=%lu softirq=%lu steal=%lu\n",
                        user, nice, sys, idle, iowait, irq, softirq, steal);
            }
            fclose(fp);
        }
    }

    /* --- 内存 --- */
    {
        FILE *fp = fopen("/proc/meminfo", "r");
        if (fp) {
            char line[256];
            long mem_total = 0, mem_free = 0, mem_avail = 0;
            long mem_buffers = 0, mem_cached = 0;
            long swap_total = 0, swap_free = 0;

            while (fgets(line, sizeof(line), fp)) {
                long val;
                if (sscanf(line, "MemTotal: %ld kB", &val) == 1) mem_total = val;
                else if (sscanf(line, "MemFree: %ld kB", &val) == 1) mem_free = val;
                else if (sscanf(line, "MemAvailable: %ld kB", &val) == 1) mem_avail = val;
                else if (sscanf(line, "Buffers: %ld kB", &val) == 1) mem_buffers = val;
                else if (sscanf(line, "Cached: %ld kB", &val) == 1) mem_cached = val;
                else if (sscanf(line, "SwapTotal: %ld kB", &val) == 1) swap_total = val;
                else if (sscanf(line, "SwapFree: %ld kB", &val) == 1) swap_free = val;
            }
            fclose(fp);

            long mem_used = mem_total - mem_free - mem_buffers - mem_cached;
            long swap_used = swap_total - swap_free;

            fprintf(out, "\n内存:\n");
            fprintf(out, "  总计: %ld MB, 已用: %ld MB, 空闲: %ld MB\n",
                    mem_total / 1024, mem_used / 1024, mem_free / 1024);
            fprintf(out, "  可用: %ld MB, Buffers: %ld MB, Cached: %ld MB\n",
                    mem_avail / 1024, mem_buffers / 1024, mem_cached / 1024);
            fprintf(out, "  Swap: 总计 %ld MB, 已用 %ld MB, 空闲 %ld MB\n",
                    swap_total / 1024, swap_used / 1024, swap_free / 1024);
        }
    }

    /* --- 负载 --- */
    {
        FILE *fp = fopen("/proc/loadavg", "r");
        if (fp) {
            double load1, load5, load15;
            if (fscanf(fp, "%lf %lf %lf", &load1, &load5, &load15) == 3) {
                fprintf(out, "\n系统负载: %.2f (1min)  %.2f (5min)  %.2f (15min)\n",
                        load1, load5, load15);
            }
            fclose(fp);
        }
    }

    /* --- 运行时间 --- */
    {
        unsigned long long uptime_secs;
        FILE *fp = fopen("/proc/uptime", "r");
        if (fp) {
            if (fscanf(fp, "%llu", &uptime_secs) == 1) {
                unsigned long days = uptime_secs / 86400;
                unsigned long hours = (uptime_secs % 86400) / 3600;
                unsigned long mins = (uptime_secs % 3600) / 60;
                fprintf(out, "\n运行时间: %lu天 %02lu:%02lu\n", days, hours, mins);
            }
            fclose(fp);
        }
    }

    fprintf(out, "\n========================================\n");
    fprintf(out, " 报告结束\n");
    fprintf(out, "========================================\n");

    fclose(out);
    printf("monitor: 监控数据已保存到 '%s'\n", args[2]);
    return 0;
}

/* ---- print_monitor_help ---- */
void print_monitor_help(void) {
    printf("用法: monitor <子命令> [参数]\n\n");
    printf("子命令列表:\n");
    for (int i = 0; monitor_commands[i].name != NULL; i++) {
        printf("  %-16s %s\n", monitor_commands[i].name, monitor_commands[i].description);
    }
    printf("\n示例:\n");
    printf("  monitor             显示系统概览\n");
    printf("  monitor overview    显示系统概览\n");
    printf("  monitor process     显示所有进程\n");
    printf("  monitor process bash    显示名为 bash 的进程\n");
    printf("  monitor process --sort-rss  按内存排序\n");
    printf("  monitor memory      显示详细内存信息\n");
    printf("  monitor network     显示网络流量\n");
    printf("  monitor filesystem  显示磁盘和挂载信息\n");
    printf("  monitor device      显示设备列表\n");
    printf("  monitor power       显示CPU频率信息\n");
    printf("  monitor save out.txt    导出监控数据\n");
}

/* ---- builtin_monitor (入口点，被 exec.c 调用) ---- */
int builtin_monitor(char **args) {
    /* args[0] = "monitor", args[1] = 子命令, args[2...] = 参数 */
    return monitor_dispatch(args);
}

/* ---- monitor_dispatch ---- */
int monitor_dispatch(char **args) {
    if (args[0] == NULL) {
        print_monitor_help();
        return 0;
    }

    /* 处理 --help */
    if (args[1] != NULL && strcmp(args[1], "--help") == 0) {
        print_monitor_help();
        return 0;
    }

    /* 无子命令 -> 默认 overview */
    if (args[1] == NULL) {
        return monitor_overview(args);
    }

    const char *subcmd = args[1];

    /* 查找子命令 */
    for (int i = 0; monitor_commands[i].name != NULL; i++) {
        if (strcmp(subcmd, monitor_commands[i].name) == 0) {
            return monitor_commands[i].handler(args);
        }
    }

    /* 未知子命令 */
    fprintf(stderr, "monitor: 未知子命令 '%s'\n", subcmd);
    fprintf(stderr, "输入 'monitor --help' 查看用法。\n");
    return 1;
}
