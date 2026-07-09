// oscd/oscdsh.h
#ifndef OSCDSH_H
#define OSCDSH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>          // 用于 open()
#include <errno.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define PROMPT "oscdsh> "

int builtin_hello(char **args);
int builtin_exit(char **args);

int execute_external(char **args);          // 原外部命令执行（未使用重定向）
int execute_command(char *line);            // 总入口，已重构

#endif