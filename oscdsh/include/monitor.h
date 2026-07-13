/*
 * monitor.h — OSCD Shell system monitoring module
 *
 * Declares all data structures and function prototypes for the monitor
 * subsystem, which provides CPU, memory, process, network, filesystem,
 * device, and power monitoring commands integrated as oscdsh builtins.
 *
 * Data sources: /proc, /sys, /proc/diskstats, /proc/net/dev, etc.
 */

#ifndef MONITOR_H
#define MONITOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/statvfs.h>

/* ── Command handlers ──────────────────────────────────── */
/* All return int (0 = success, 1 = error) and take NULL-   *
 * terminated argument vector (args[0] = command name).     */

int monitor_overview(char **args);      /* CPU / memory / load overview     */
int monitor_process(char **args);       /* Process list from /proc/[pid]    */
int monitor_memory(char **args);        /* Memory usage + SLUB cache        */
int monitor_network(char **args);       /* Network traffic per interface    */
int monitor_filesystem(char **args);    /* Disk I/O + mount points          */
int monitor_device(char **args);        /* Device list (/proc/devices, /sys)*/
int monitor_power(char **args);         /* CPU frequency / power stats      */
int monitor_save(char **args);          /* Export monitored data to file    */

/* ── Dispatch structure ────────────────────────────────── */

typedef struct {
    const char *name;                   /* Command name (e.g. "overview")   */
    int (*handler)(char **args);        /* Handler function pointer         */
    const char *description;            /* One-line help text               */
} MonitorCommand;

/* ── Command table (definition in monitor.c) ──────────── */

extern MonitorCommand monitor_commands[];

/* ── Dispatch and help ────────────────────────────────── */

int monitor_dispatch(char **args);      /* Route args[0] to handler         */
void print_monitor_help(void);          /* Print available monitor commands */

#endif /* MONITOR_H */
