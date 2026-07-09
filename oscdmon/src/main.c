#include "oscdmon.h"

/* 读取 /proc/loadavg 并打印负载 */
static void print_loadavg(void) {
    FILE *fp = fopen("/proc/loadavg", "r");
    if (!fp) {
        perror("fopen /proc/loadavg");
        return;
    }
    double load1, load5, load15;
    fscanf(fp, "%lf %lf %lf", &load1, &load5, &load15);
    fclose(fp);
    printf("负载: %.2f (1min)  %.2f (5min)  %.2f (15min)\n", load1, load5, load15);
}

/* 读取 /proc/meminfo 并打印内存信息 */
static void print_memory(void) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("fopen /proc/meminfo");
        return;
    }
    char line[256];
    long mem_total = 0, mem_free = 0, mem_available = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0)
            sscanf(line + 9, "%ld", &mem_total);
        else if (strncmp(line, "MemFree:", 8) == 0)
            sscanf(line + 8, "%ld", &mem_free);
        else if (strncmp(line, "MemAvailable:", 13) == 0)
            sscanf(line + 13, "%ld", &mem_available);
    }
    fclose(fp);
    printf("内存: 总计 %ld MB, 空闲 %ld MB, 可用 %ld MB\n",
           mem_total / 1024, mem_free / 1024, mem_available / 1024);
}

/* 读取 /proc/stat 并打印简单 CPU 使用率（基于采样） */
static void print_cpu_usage(void) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("fopen /proc/stat");
        return;
    }
    char line[256];
    fgets(line, sizeof(line), fp);  // 第一行是 cpu 汇总
    fclose(fp);

    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
    sscanf(line, "cpu  %lu %lu %lu %lu %lu %lu %lu %lu",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

    unsigned long total = user + nice + system + idle + iowait + irq + softirq + steal;
    unsigned long idle_total = idle + iowait;
    double usage = 100.0 * (total - idle_total) / total;

    printf("CPU 使用率: %.1f%%\n", usage);
}

/* 打印系统概览（一次） */
void print_overview(void) {
    printf("OSCD monitor\n");

    print_cpu_usage();
    print_memory();
    print_loadavg();

    printf("\n");
}

int main(void) {
    print_overview();
    return 0;
}