// oscdsh/exec.c
#include "oscdsh.h"

/* ---------- 内置命令查找 ---------- */
static int execute_builtin(char **args) {
    if (strcmp(args[0], "hello") == 0) return builtin_hello(args);
    if (strcmp(args[0], "exit") == 0)  return builtin_exit(args);
    return -1;
}

/* ---------- 执行单一命令（支持输出重定向） ---------- */
static int execute_single(char **args, char *outfile, int append) {
    pid_t pid = fork();
    if (pid == 0) {
        // 子进程：处理输出重定向
        if (outfile) {
            int flags = O_WRONLY | O_CREAT;
            flags |= append ? O_APPEND : O_TRUNC;
            int fd = open(outfile, flags, 0644);
            if (fd < 0) {
                perror("oscdsh: open");
                exit(1);
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {
                perror("oscdsh: dup2");
                close(fd);
                exit(1);
            }
            close(fd);
        }
        execvp(args[0], args);
        fprintf(stderr, "oscdsh: %s: 命令未找到\n", args[0]);
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    } else {
        perror("oscdsh: fork 失败");
        return -1;
    }
}

/* ---------- 总入口：解析并执行 ---------- */
int execute_command(char *line) {
    // 0. 注释处理：截断第一个 #（简化实现）
    char *comment = strchr(line, '#');
    if (comment) {
        *comment = '\0';
        // 去掉截断后末尾的空白字符
        int len = strlen(line);
        while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\t')) {
            line[len-1] = '\0';
            len--;
        }
    }

    // 1. 简单分词（按空格和制表符）
    char *args[MAX_ARGS];
    int argc = 0;
    char *token = strtok(line, " \t");
    while (token && argc < MAX_ARGS - 1) {
        args[argc++] = token;
        token = strtok(NULL, " \t");
    }
    args[argc] = NULL;
    if (argc == 0) return 0;

    // 2. 扫描重定向符号（> 和 >>），跳过已置 NULL 的项
    char *outfile = NULL;
    int append = 0;
    for (int i = 0; i < argc; i++) {
        if (args[i] == NULL)
            continue;

        if (strcmp(args[i], ">") == 0) {
            if (i + 1 < argc && args[i+1] != NULL) {
                outfile = args[i + 1];
                args[i] = NULL;
                args[i + 1] = NULL;
                append = 0;
            } else {
                fprintf(stderr, "oscdsh: 语法错误: 缺少输出文件名\n");
                return -1;
            }
        } else if (strcmp(args[i], ">>") == 0) {
            if (i + 1 < argc && args[i+1] != NULL) {
                outfile = args[i + 1];
                args[i] = NULL;
                args[i + 1] = NULL;
                append = 1;
            } else {
                fprintf(stderr, "oscdsh: 语法错误: 缺少输出文件名\n");
                return -1;
            }
        }
    }

    // 3. 压缩参数列表（去掉已置 NULL 的 token）
    char *new_args[MAX_ARGS];
    int j = 0;
    for (int i = 0; i < argc; i++) {
        if (args[i] != NULL)
            new_args[j++] = args[i];
    }
    new_args[j] = NULL;

    // 4. 处理空命令（例如仅输入 > file）
    if (j == 0) {
        if (outfile) {
            int fd = open(outfile, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
            if (fd < 0)
                perror("oscdsh: open");
            else
                close(fd);
        }
        return 0;
    }

    // 5. 暂不支持的功能提示
    for (int i = 0; i < j; i++) {
        if (strcmp(new_args[i], "|") == 0) {
            fprintf(stderr, "oscdsh: 管道功能尚未实现\n");
            return -1;
        }
    }
    for (int i = 0; i < j; i++) {
        if (strcmp(new_args[i], "<") == 0) {
            fprintf(stderr, "oscdsh: 输入重定向尚未实现\n");
            return -1;
        }
    }
    if (j > 0 && strcmp(new_args[j-1], "&") == 0) {
        fprintf(stderr, "oscdsh: 后台运行尚未实现\n");
        return -1;
    }

    // 6. 尝试内置命令
    int ret = execute_builtin(new_args);
    if (ret != -1) return ret;

    // 7. 外部命令（带输出重定向）
    return execute_single(new_args, outfile, append);
}