// oscdfs/include/oscdfs.h
#ifndef OSCDFS_H
#define OSCDFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CMD_LEN 1024
#define PROMPT "oscdfs> "

/* 内置命令：返回0表示成功，-1表示失败 */
int builtin_hello(char **args);
int builtin_exit(char **args);

/* 解析并执行一行命令 */
int execute_command(char *line);

#endif