// src/exec.c
#include "oscdsh.h"

#define MAX_PIPES 16

/* 公共：内置命令判定 */
int is_builtin_cmd(const char *cmd) {
    if (strcmp(cmd, "hello") == 0)   return 1;
    if (strcmp(cmd, "exit") == 0)    return 1;
    if (strcmp(cmd, "cd") == 0)      return 1;
    if (strcmp(cmd, "type") == 0)    return 1;
    if (strcmp(cmd, "history") == 0) return 1;
    if (strcmp(cmd, "alias") == 0)   return 1;
    if (strcmp(cmd, "unalias") == 0) return 1;
    if (strcmp(cmd, "pwd") == 0)     return 1;
    if (strcmp(cmd, "export") == 0)  return 1;
    if (strcmp(cmd, "jobs") == 0)    return 1;
    return 0;
}

static int execute_builtin(char **args) {
    /* 统一处理 --help 选项 */
    if (args[1] != NULL && strcmp(args[1], "--help") == 0) {
        builtin_help(args[0]);
        return 1;
    }

    if (strcmp(args[0], "hello") == 0)   return builtin_hello(args);
    if (strcmp(args[0], "exit") == 0)    return builtin_exit(args);
    if (strcmp(args[0], "cd") == 0)      return builtin_cd(args);
    if (strcmp(args[0], "type") == 0)    return builtin_type(args);
    if (strcmp(args[0], "history") == 0) return builtin_history(args);
    if (strcmp(args[0], "alias") == 0)   return builtin_alias(args);
    if (strcmp(args[0], "unalias") == 0) return builtin_unalias(args);
    if (strcmp(args[0], "pwd") == 0)     return builtin_pwd(args);
    if (strcmp(args[0], "export") == 0)  return builtin_export(args);
    if (strcmp(args[0], "jobs") == 0)    return builtin_jobs(args);
    return -1;
}

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

static int execute_single(char **args, char *infile, char *outfile, int append,
                          int background, const char *raw_cmdline)
{
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
        if (background) {
            int jid = add_job(pid, raw_cmdline);
            if (jid != -1)
                printf("[%d] %d\n", jid, pid);
            return 0;
        } else {
            int status;
            waitpid(pid, &status, 0);
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }
    } else {
        perror("oscdsh: fork 失败");
        return -1;
    }
}

static int execute_pipeline(char *cmd_strings[], int n, int background, const char *raw_cmdline) {
    int prev_pipe[2] = {-1, -1};
    pid_t first_pid = -1;

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

        if (i == 0) first_pid = pid;

        if (i > 0) {
            close(prev_pipe[0]);
            close(prev_pipe[1]);
        }
        if (i < n - 1) {
            prev_pipe[0] = cur_pipe[0];
            prev_pipe[1] = cur_pipe[1];
        }
    }

    if (background) {
        if (first_pid > 0) {
            int jid = add_job(first_pid, raw_cmdline);
            if (jid != -1)
                printf("[%d] %d\n", jid, first_pid);
        }
        return 0;
    } else {
        while (wait(NULL) > 0);
        return 0;
    }
}

static int is_blank(const char *s) {
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

int execute_command(char *line) {
    char *raw_cmd = strdup(line);
    if (!raw_cmd) return -1;

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

    // 1. 提取后台 &
    int background = 0;
    int len = strlen(line);
    while (len > 0 && isspace((unsigned char)line[len-1])) {
        line[len-1] = '\0';
        len--;
    }
    if (len > 0 && line[len-1] == '&') {
        background = 1;
        line[len-1] = '\0';
        len--;
        while (len > 0 && isspace((unsigned char)line[len-1])) {
            line[len-1] = '\0';
            len--;
        }
    }

    // 2. 检查多余 '&'
    if (strchr(line, '&') != NULL) {
        fprintf(stderr, "oscdsh: 语法错误: 多余的 '&'\n");
        free(raw_cmd);
        return -1;
    }

    if (background && strlen(line) == 0) {
        fprintf(stderr, "oscdsh: 语法错误: 命令不完整\n");
        free(raw_cmd);
        return -1;
    }

    // 3. 管道分割
    char *cmds[MAX_PIPES];
    int n = 0;
    char *start = line;
    char *p = line;
    while (*p && n < MAX_PIPES) {
        if (*p == '|') {
            *p = '\0';
            cmds[n++] = start;
            start = p + 1;
        }
        p++;
    }
    if (n < MAX_PIPES) {
        cmds[n++] = start;
    } else {
        fprintf(stderr, "oscdsh: 管道段数超过上限 %d\n", MAX_PIPES);
        free(raw_cmd);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        if (is_blank(cmds[i])) {
            if (i == 0) {
                fprintf(stderr, "oscdsh: 语法错误: 管道符前缺少命令\n");
                free(raw_cmd);
                return -1;
            } else if (i == n-1 && n > 1) {
                fprintf(stderr, "oscdsh: 语法错误: 管道符后缺少命令\n");
                free(raw_cmd);
                return -1;
            } else {
                fprintf(stderr, "oscdsh: 语法错误: 连续管道符或空命令\n");
                free(raw_cmd);
                return -1;
            }
        }
    }

    if (n == 1) {
        char *args[MAX_ARGS];
        char *infile = NULL, *outfile = NULL;
        int append = 0;
        int ret = parse_single_cmd(cmds[0], args, &infile, &outfile, &append);
        if (ret < 0) {
            if (outfile) {
                int fd = open(outfile, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
                if (fd < 0) perror("oscdsh: open");
                else close(fd);
            }
            free(raw_cmd);
            return 0;
        }

        if (background && args[0] != NULL && is_builtin_cmd(args[0])) {
            fprintf(stderr, "oscdsh: 内置命令不能后台运行\n");
            free(raw_cmd);
            return -1;
        }

        if (!background) {
            int builtin_ret = execute_builtin(args);
            if (builtin_ret != -1) {
                free(raw_cmd);
                return builtin_ret;
            }
        }

        int result = execute_single(args, infile, outfile, append, background, raw_cmd);
        free(raw_cmd);
        return result;
    } else {
        for (int i = 0; i < n; i++) {
            char temp[1024];
            strncpy(temp, cmds[i], sizeof(temp)-1);
            temp[sizeof(temp)-1] = '\0';
            char *first = strtok(temp, " \t");
            if (first != NULL && is_builtin_cmd(first)) {
                fprintf(stderr, "oscdsh: 管道中不能使用内置命令\n");
                free(raw_cmd);
                return -1;
            }
        }
        int result = execute_pipeline(cmds, n, background, raw_cmd);
        free(raw_cmd);
        return result;
    }
}