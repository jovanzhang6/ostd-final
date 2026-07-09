// include/oscdsh.h
#ifndef OSCDSH_H
#define OSCDSH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define MAX_PIPES 16
#define PROMPT "oscdsh> "

/* 内置命令 */
int builtin_hello(char **args);
int builtin_exit(char **args);

/* 执行入口 */
int execute_command(char *line);

/* 作业管理 */
void init_jobs(void);
void sigchld_handler(int sig);

#endif