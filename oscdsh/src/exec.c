#include "oscdsh.h"

/* 查找并执行内置命令 */
static int execute_builtin(char **args) {
    if (strcmp(args[0], "hello") == 0) return builtin_hello(args);
    if (strcmp(args[0], "exit") == 0)  return builtin_exit(args);
    return -1;  // 不是内置命令
}

/* 执行外部命令 */
int execute_external(char **args) {
    pid_t pid = fork();
    if (pid == 0) {
        // 子进程执行
        execvp(args[0], args);
        // 如果 execvp 返回，说明出错了
        fprintf(stderr, "oscdsh: %s: 命令未找到\n", args[0]);
        exit(1);
    } else if (pid > 0) {
        // 父进程等待子进程结束
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    } else {
        perror("fork 失败");
        return -1;
    }
}

/* 总入口：解析命令并执行 */
int execute_command(char *line) {
    // 1. 简单分词（按空格切分）
    char *args[MAX_ARGS];
    int argc = 0;
    char *token = strtok(line, " ");
    while (token && argc < MAX_ARGS - 1) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }
    args[argc] = NULL;

    if (argc == 0) return 0;

    // 2. 尝试执行内置命令
    int ret = execute_builtin(args);
    if (ret != -1) return ret;

    // 3. 执行外部命令
    return execute_external(args);
}