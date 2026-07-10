// oscdfs/src/exec.c
#include "oscdfs.h"

static int execute_builtin(char **args) {
    if (strcmp(args[0], "hello") == 0)  return builtin_hello(args);
    if (strcmp(args[0], "exit") == 0)   return builtin_exit(args);
    if (strcmp(args[0], "dir") == 0)    return builtin_dir(args);
    if (strcmp(args[0], "cd") == 0)     return builtin_cd(args);
    if (strcmp(args[0], "create") == 0) return builtin_create(args);
    if (strcmp(args[0], "open") == 0)   return builtin_open(args);
    if (strcmp(args[0], "close") == 0)  return builtin_close(args);
    if (strcmp(args[0], "read") == 0)   return builtin_read(args);
    if (strcmp(args[0], "write") == 0)  return builtin_write(args);
    if (strcmp(args[0], "delete") == 0) return builtin_delete(args);
    if (strcmp(args[0], "login") == 0)  return builtin_login(args);
    if (strcmp(args[0], "chmod") == 0)  return builtin_chmod(args);
    if (strcmp(args[0], "chown") == 0)  return builtin_chown(args);
    return -1;
}

int execute_command(char *line) {
    char *args[16];
    int argc = 0;
    char *token = strtok(line, " \t");
    while (token && argc < 15) {
        args[argc++] = token;
        token = strtok(NULL, " \t");
    }
    args[argc] = NULL;
    if (token != NULL) {
        fprintf(stderr, "oscdfs: 警告：命令参数过多，仅解析前15个参数\n");
    }
    if (argc == 0) return 0;
    int ret = execute_builtin(args);
    if (ret != -1) return ret;
    fprintf(stderr, "oscdfs: 未知命令 '%s'\n", args[0]);
    return -1;
}