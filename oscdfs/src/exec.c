// oscdfs/src/exec.c
#include "oscdfs.h"

/* 查找并执行内置命令，成功返回0，未找到返回-1 */
static int execute_builtin(char **args) {
    if (strcmp(args[0], "hello") == 0) return builtin_hello(args);
    if (strcmp(args[0], "exit") == 0)  return builtin_exit(args);
    return -1;
}

int execute_command(char *line) {
    char *args[16];
    int argc = 0;

    /* 以空格或制表符分词，连续分隔符视为一个 */
    char *token = strtok(line, " \t");
    while (token && argc < 15) {
        args[argc++] = token;
        token = strtok(NULL, " \t");
    }
    args[argc] = NULL;

    /* 如果还有未解析的单词，说明参数过多，给出警告 */
    if (token != NULL) {
        fprintf(stderr, "oscdfs: 警告：命令参数过多，仅解析前15个参数\n");
    }

    if (argc == 0)
        return 0;

    int ret = execute_builtin(args);
    if (ret != -1)
        return ret;

    /* 未知命令 */
    fprintf(stderr, "oscdfs: 未知命令 '%s'\n", args[0]);
    return -1;
}