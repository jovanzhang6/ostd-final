#include "oscdvfs.h"

/* 查找并执行内置命令 */
static int execute_builtin(char **args) {
    if (strcmp(args[0], "hello") == 0) return builtin_hello(args);
    if (strcmp(args[0], "exit") == 0)  return builtin_exit(args);
    return -1;
}

int execute_command(char *line) {
    // 简单分词
    char *args[16];
    int argc = 0;
    char *token = strtok(line, " ");
    while (token && argc < 15) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }
    args[argc] = NULL;

    if (argc == 0) return 0;

    int ret = execute_builtin(args);
    if (ret != -1) return ret;

    // 未知命令
    fprintf(stderr, "oscdvfs: 未知命令 '%s'\n", args[0]);
    return -1;
}