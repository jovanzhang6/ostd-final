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
#define MAX_HISTORY 1024
#define MAX_ALIASES 64
#define PROMPT "oscdsh> "

/* 内置命令 */
int builtin_hello(char **args);
int builtin_exit(char **args);
int builtin_cd(char **args);
int builtin_type(char **args);
int builtin_history(char **args);
int builtin_alias(char **args);
int builtin_unalias(char **args);
int builtin_pwd(char **args);
int builtin_export(char **args);

/* 执行入口 */
int execute_command(char *line);

/* 公共工具 */
int is_builtin_cmd(const char *cmd);

/* 作业管理 */
void init_jobs(void);
void sigchld_handler(int sig);

/* 历史管理 */
void add_history(const char *cmd);
char *expand_history(const char *input);
void print_history(void);

/* 别名管理 */
void add_alias(const char *name, const char *value);
int  remove_alias(const char *name);
const char *get_alias_value(const char *name);
void print_aliases(void);
void print_one_alias(const char *name);

#endif