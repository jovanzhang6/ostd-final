// oscdsh/include/oscdsh.h
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
#include <dirent.h>
#include <signal.h>

#include <readline/readline.h>
#include <readline/history.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define MAX_PIPES 16
#define MAX_HISTORY 1024
#define MAX_ALIASES 64
#define MAX_JOBS 64

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
int builtin_jobs(char **args);
int builtin_true(char **args);
int builtin_false(char **args);

/* 执行入口 */
int execute_command(char *line);

/* 公共工具 */
int is_builtin_cmd(const char *cmd);
void builtin_help(const char *cmd);

/* 作业管理 */
void init_jobs(void);
int  add_job(pid_t pid, const char *cmdline);
void sigchld_handler(int sig);
void print_jobs(void);

/* 历史管理 */
void hist_add(const char *cmd);
char *expand_history(const char *input);
void print_history(void);
void load_history(void);
void save_history(void);
void clear_history(void);

/* 别名管理 */
void add_alias(const char *name, const char *value);
int  remove_alias(const char *name);
const char *get_alias_value(const char *name);
void print_aliases(void);
void print_one_alias(const char *name);

/* Tab 补全 */
char **oscdsh_completion(const char *text, int start, int end);

/* 动态提示符 */
char *get_prompt(void);

#endif