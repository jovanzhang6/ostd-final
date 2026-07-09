// oscdsh/exec.c
#include "oscdsh.h"
#include <ctype.h>    // 为 isspace

#define MAX_PIPES 16

/* ---------- 内置命令查找 ---------- */
static int execute_builtin(char **args) {
    if (strcmp(args[0], "hello") == 0) return builtin_hello(args);
    if (strcmp(args[0], "exit") == 0)  return builtin_exit(args);
    return -1;
}

static int is_builtin(char *cmd) {
    if (strcmp(cmd, "hello") == 0) return 1;
    if (strcmp(cmd, "exit") == 0)  return 1;
    return 0;
}

/* ---------- 解析单个命令段：分词 + 提取重定向 ---------- */
static int parse_single_cmd(char *cmdline, char **args_out,
                            char **infile_out, char **outfile_out, int *append_out)
{
    char *args[MAX_ARGS];
    int argc = 0;
    char *token = strtok(cmdline, " \t");
    while (token && argc < MAX_ARGS - 1) {
        args[argc++] = token;
        token = strtok(NULL, " \t");
    }
    args[argc] = NULL;
    if (argc == 0) return -1;

    char *infile = NULL, *outfile = NULL;
    int append = 0;
    for (int i = 0; i < argc; i++) {
        if (args[i] == NULL) continue;

        if (strcmp(args[i], ">") == 0) {
            if (i + 1 < argc && args[i+1] != NULL) {
                outfile = args[i+1];
                args[i] = NULL;
                args[i+1] = NULL;
                append = 0;
            } else {
                fprintf(stderr, "oscdsh: 语法错误: 缺少输出文件名\n");
                return -1;
            }
        } else if (strcmp(args[i], ">>") == 0) {
            if (i + 1 < argc && args[i+1] != NULL) {
                outfile = args[i+1];
                args[i] = NULL;
                args[i+1] = NULL;
                append = 1;
            } else {
                fprintf(stderr, "oscdsh: 语法错误: 缺少输出文件名\n");
                return -1;
            }
        } else if (strcmp(args[i], "<") == 0) {
            if (i + 1 < argc && args[i+1] != NULL) {
                infile = args[i+1];
                args[i] = NULL;
                args[i+1] = NULL;
            } else {
                fprintf(stderr, "oscdsh: 语法错误: 缺少输入文件名\n");
                return -1;
            }
        }
    }

    int j = 0;
    for (int i = 0; i < argc; i++) {
        if (args[i] != NULL)
            args_out[j++] = args[i];
    }
    args_out[j] = NULL;

    *infile_out = infile;
    *outfile_out = outfile;
    *append_out = append;
    return j;
}

/* ---------- 执行单一命令（带重定向） ---------- */
static int execute_single(char **args, char *infile, char *outfile, int append) {
    pid_t pid = fork();
    if (pid == 0) {
        if (infile) {
            int fd = open(infile, O_RDONLY);
            if (fd < 0) { perror("oscdsh: open infile"); exit(1); }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (outfile) {
            int flags = O_WRONLY | O_CREAT;
            flags |= append ? O_APPEND : O_TRUNC;
            int fd = open(outfile, flags, 0644);
            if (fd < 0) { perror("oscdsh: open outfile"); exit(1); }
            dup2(fd, STDOUT_FILENO);
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

/* ---------- 执行管道链 ---------- */
static int execute_pipeline(char *cmd_strings[], int n) {
    int prev_pipe[2] = {-1, -1};

    for (int i = 0; i < n; i++) {
        int cur_pipe[2];
        if (i < n - 1) {
            if (pipe(cur_pipe) == -1) {
                perror("oscdsh: pipe");
                return -1;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            if (i > 0) {
                dup2(prev_pipe[0], STDIN_FILENO);
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }
            if (i < n - 1) {
                dup2(cur_pipe[1], STDOUT_FILENO);
                close(cur_pipe[0]);
                close(cur_pipe[1]);
            }

            char *args[MAX_ARGS];
            char *infile = NULL, *outfile = NULL;
            int append = 0;
            if (parse_single_cmd(cmd_strings[i], args, &infile, &outfile, &append) < 0) {
                exit(0);
            }
            if (infile) {
                int fd = open(infile, O_RDONLY);
                if (fd < 0) { perror("oscdsh: open infile"); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (outfile) {
                int flags = O_WRONLY | O_CREAT;
                flags |= append ? O_APPEND : O_TRUNC;
                int fd = open(outfile, flags, 0644);
                if (fd < 0) { perror("oscdsh: open outfile"); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            execvp(args[0], args);
            fprintf(stderr, "oscdsh: %s: 命令未找到\n", args[0]);
            exit(1);
        }

        if (i > 0) {
            close(prev_pipe[0]);
            close(prev_pipe[1]);
        }
        if (i < n - 1) {
            prev_pipe[0] = cur_pipe[0];
            prev_pipe[1] = cur_pipe[1];
        }
    }

    while (wait(NULL) > 0);
    return 0;
}

/* ---------- 辅助：字符串是否完全空白 ---------- */
static int is_blank(const char *s) {
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* ---------- 总入口 ---------- */
int execute_command(char *line) {
    // 0. 注释处理
    char *comment = strchr(line, '#');
    if (comment) {
        *comment = '\0';
        int len = strlen(line);
        while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\t')) {
            line[len-1] = '\0';
            len--;
        }
    }

    // 1. 手动按 | 分割命令段，保留空段
    char *cmds[MAX_PIPES];
    int n = 0;
    char *start = line;
    char *p = line;
    int in_quotes = 0;   // 未实现，保留接口
    while (*p && n < MAX_PIPES) {
        if (*p == '|' && !in_quotes) {
            // 截断出一个段
            *p = '\0';
            cmds[n++] = start;
            start = p + 1;
        }
        p++;
    }
    // 最后一段
    if (n < MAX_PIPES) {
        cmds[n++] = start;
    } else {
        fprintf(stderr, "oscdsh: 管道段数超过上限 %d\n", MAX_PIPES);
        return -1;
    }

    // 2. 检查管道语法错误（空段）
    for (int i = 0; i < n; i++) {
        // 去除首尾空白后判断是否为空
        char *seg = cmds[i];
        while (isspace((unsigned char)*seg)) seg++;
        // 尾部空白已通过注释处理阶段去掉？没有，这里是段，需要自己处理
        // 简化：直接用 is_blank 判断整个段（不去掉空白，因为可能全为空白）
        if (is_blank(cmds[i])) {
            if (i == 0) {
                fprintf(stderr, "oscdsh: 语法错误: 管道符前缺少命令\n");
                return -1;
            } else if (i == n-1 && n > 1) {
                fprintf(stderr, "oscdsh: 语法错误: 管道符后缺少命令\n");
                return -1;
            } else {
                fprintf(stderr, "oscdsh: 语法错误: 连续管道符或空命令\n");
                return -1;
            }
        }
    }

    // 3. 单命令或多命令分发
    if (n == 1) {
        // 单命令分支，可包含重定向
        char *args[MAX_ARGS];
        char *infile = NULL, *outfile = NULL;
        int append = 0;
        int ret = parse_single_cmd(cmds[0], args, &infile, &outfile, &append);
        if (ret < 0) {
            // 空命令，但可能有重定向（如 > file）
            if (outfile) {
                int fd = open(outfile, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
                if (fd < 0) perror("oscdsh: open");
                else close(fd);
            }
            return 0;
        }

        // 后台检查
        int last = 0;
        while (args[last] != NULL) last++;
        if (last > 0 && strcmp(args[last-1], "&") == 0) {
            fprintf(stderr, "oscdsh: 后台运行尚未实现\n");
            return -1;
        }

        // 内置命令
        int builtin_ret = execute_builtin(args);
        if (builtin_ret != -1) return builtin_ret;

        return execute_single(args, infile, outfile, append);
    } else {
        // 管道：检查每个段的首命令是否为内置命令
        for (int i = 0; i < n; i++) {
            // 提取第一个单词
            char temp[1024];
            strncpy(temp, cmds[i], sizeof(temp)-1);
            temp[sizeof(temp)-1] = '\0';
            char *first = strtok(temp, " \t");
            if (first != NULL && is_builtin(first)) {
                fprintf(stderr, "oscdsh: 管道中不能使用内置命令\n");
                return -1;
            }
        }
        return execute_pipeline(cmds, n);
    }
}