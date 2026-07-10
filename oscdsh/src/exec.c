// oscdsh/src/exec.c
#include "oscdsh.h"

#define MAX_PIPES 16
#define MAX_SEQUENCE 16

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
    if (strcmp(cmd, "true") == 0)    return 1;
    if (strcmp(cmd, "false") == 0)   return 1;
    return 0;
}

/* 内置命令执行（统一入口） */
static int execute_builtin(char **args) {
    if (args[1] != NULL && strcmp(args[1], "--help") == 0) {
        builtin_help(args[0]);
        return 0;
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
    if (strcmp(args[0], "true") == 0)    return builtin_true(args);
    if (strcmp(args[0], "false") == 0)   return builtin_false(args);
    return -1;
}

/* 辅助函数 */
static int is_blank(const char *s) {
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* 解析单个命令段：分词 + 提取重定向（支持数字文件描述符，如 2>） */
static int parse_single_cmd(char *cmdline, char **args_out,
                            char **infile_out, char **outfile_out, int *append_out,
                            int *fd_in, int *fd_out)
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
    int infd = STDIN_FILENO;
    int outfd = STDOUT_FILENO;

    for (int i = 0; i < argc; i++) {
        if (args[i] == NULL) continue;

        char *p = args[i];
        // 检查是否以数字开头，可能为带文件描述符的重定向，如 "2>" "0<"
        if (*p >= '0' && *p <= '9') {
            char *num_end = p;
            while (*num_end >= '0' && *num_end <= '9') num_end++;
            if (num_end > p && *num_end != '\0') {
                int fd_num = atoi(p);
                if (fd_num < 0 || fd_num > 2) {
                    fprintf(stderr, "oscdsh: 无效的文件描述符 %d\n", fd_num);
                    return -1;
                }
                char *op = num_end;
                if (strcmp(op, ">") == 0) {
                    if (i + 1 < argc && args[i+1] != NULL) {
                        outfile = args[i+1];
                        outfd = fd_num;
                        args[i] = NULL;
                        args[i+1] = NULL;
                        append = 0;
                    } else {
                        fprintf(stderr, "oscdsh: 语法错误: 缺少输出文件名\n");
                        return -1;
                    }
                } else if (strcmp(op, ">>") == 0) {
                    if (i + 1 < argc && args[i+1] != NULL) {
                        outfile = args[i+1];
                        outfd = fd_num;
                        args[i] = NULL;
                        args[i+1] = NULL;
                        append = 1;
                    } else {
                        fprintf(stderr, "oscdsh: 语法错误: 缺少输出文件名\n");
                        return -1;
                    }
                } else if (strcmp(op, "<") == 0) {
                    if (i + 1 < argc && args[i+1] != NULL) {
                        infile = args[i+1];
                        infd = fd_num;
                        args[i] = NULL;
                        args[i+1] = NULL;
                    } else {
                        fprintf(stderr, "oscdsh: 语法错误: 缺少输入文件名\n");
                        return -1;
                    }
                } else {
                    // 虽然以数字开头但不是重定向操作符，保留为普通参数
                    continue;
                }
            }
        }
        // 处理传统的 >, >>, <
        else if (strcmp(args[i], ">") == 0) {
            if (i + 1 < argc && args[i+1] != NULL) {
                outfile = args[i+1];
                outfd = STDOUT_FILENO;
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
                outfd = STDOUT_FILENO;
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
                infd = STDIN_FILENO;
                args[i] = NULL;
                args[i+1] = NULL;
            } else {
                fprintf(stderr, "oscdsh: 语法错误: 缺少输入文件名\n");
                return -1;
            }
        }
    }

    // 压缩参数数组，移除已被置空的项
    int j = 0;
    for (int i = 0; i < argc; i++) {
        if (args[i] != NULL)
            args_out[j++] = args[i];
    }
    args_out[j] = NULL;

    *infile_out = infile;
    *outfile_out = outfile;
    *append_out = append;
    *fd_in = infd;
    *fd_out = outfd;
    return (j == 0) ? -1 : j;
}

/* 执行单一命令（返回退出状态） */
static int execute_single(char **args, char *infile, char *outfile, int append,
                          int fd_in, int fd_out, int background, const char *raw_cmdline)
{
    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        if (infile) {
            int fd = open(infile, O_RDONLY);
            if (fd < 0) { perror("oscdsh: open infile"); exit(1); }
            dup2(fd, fd_in);
            close(fd);
        }
        if (outfile) {
            int flags = O_WRONLY | O_CREAT;
            flags |= append ? O_APPEND : O_TRUNC;
            int fd = open(outfile, flags, 0644);
            if (fd < 0) { perror("oscdsh: open outfile"); exit(1); }
            dup2(fd, fd_out);
            close(fd);
        }
        execvp(args[0], args);
        fprintf(stderr, "oscdsh: %s: 命令未找到\n", args[0]);
        exit(127);
    } else if (pid > 0) {
        if (background) {
            int jid = add_job(pid, raw_cmdline);
            if (jid != -1)
                printf("[%d] %d\n", jid, pid);
            return 0;
        } else {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status))
                return WEXITSTATUS(status);
            return 1;
        }
    } else {
        perror("oscdsh: fork 失败");
        return 1;
    }
}

/* 按管道分割命令行 */
static int split_by_pipe(char *line, char *cmds[], int max_cmds) {
    int n = 0;
    char *start = line;
    char *p = line;
    while (*p && n < max_cmds) {
        if (*p == '|') {
            *p = '\0';
            cmds[n++] = start;
            start = p + 1;
        }
        p++;
    }
    if (n < max_cmds) {
        cmds[n++] = start;
    }
    return n;
}

/* 执行管道（返回最后一个命令的退出状态） */
static int execute_pipeline(char *cmd_strings[], int n, int background, const char *raw_cmdline) {
    for (int i = 0; i < n; i++) {
        char temp[1024];
        strncpy(temp, cmd_strings[i], sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';
        char *first = strtok(temp, " \t");
        if (first != NULL && is_builtin_cmd(first)) {
            fprintf(stderr, "oscdsh: 管道中不能使用内置命令\n");
            return 1;
        }
    }
    int prev_pipe[2] = {-1, -1};
    pid_t last_pid = -1;

    for (int i = 0; i < n; i++) {
        int cur_pipe[2];
        if (i < n - 1) {
            if (pipe(cur_pipe) == -1) {
                perror("oscdsh: pipe");
                return 1;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
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
            int fd_in = STDIN_FILENO, fd_out = STDOUT_FILENO;
            if (parse_single_cmd(cmd_strings[i], args, &infile, &outfile, &append,
                                 &fd_in, &fd_out) < 0) {
                exit(0);
            }
            if (infile) {
                int fd = open(infile, O_RDONLY);
                if (fd < 0) { perror("oscdsh: open infile"); exit(1); }
                dup2(fd, fd_in);
                close(fd);
            }
            if (outfile) {
                int flags = O_WRONLY | O_CREAT;
                flags |= append ? O_APPEND : O_TRUNC;
                int fd = open(outfile, flags, 0644);
                if (fd < 0) { perror("oscdsh: open outfile"); exit(1); }
                dup2(fd, fd_out);
                close(fd);
            }

            execvp(args[0], args);
            fprintf(stderr, "oscdsh: %s: 命令未找到\n", args[0]);
            exit(127);
        }

        if (i == n - 1) last_pid = pid;

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
        if (last_pid > 0) {
            int jid = add_job(last_pid, raw_cmdline);
            if (jid != -1)
                printf("[%d] %d\n", jid, last_pid);
        }
        return 0;
    } else {
        int status;
        int ret = 1;               // 默认失败
        pid_t wpid;
        // 逐个等待所有管道子进程，并记录最后一个命令的退出状态
        while ((wpid = wait(&status)) > 0) {
            if (wpid == last_pid) {
                if (WIFEXITED(status))
                    ret = WEXITSTATUS(status);
                else
                    ret = 1;       // 被信号终止等视为失败
            }
        }
        return ret;
    }
}

/* 执行一个命令段（可能是单命令或管道），返回退出状态 */
static int execute_pipeline_or_single(char *cmdline) {
    char *cmds[MAX_PIPES];
    int n = split_by_pipe(cmdline, cmds, MAX_PIPES);

    if (n == 1) {
        char *args[MAX_ARGS];
        char *infile = NULL, *outfile = NULL;
        int append = 0;
        int fd_in = STDIN_FILENO, fd_out = STDOUT_FILENO;
        if (parse_single_cmd(cmds[0], args, &infile, &outfile, &append,
                             &fd_in, &fd_out) < 0) {
            if (outfile) {
                int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
                int fd = open(outfile, flags, 0644);
                if (fd < 0) return 1;
                close(fd);
            }
            return 0;
        }
        int builtin_ret = execute_builtin(args);
        if (builtin_ret != -1)
            return builtin_ret;
        return execute_single(args, infile, outfile, append, fd_in, fd_out, 0, NULL);
    } else {
        return execute_pipeline(cmds, n, 0, NULL);
    }
}

/* 逻辑运算符分割 */
enum { OP_NONE, OP_AND, OP_OR };

static int split_by_logic(char *line, char *segments[], int operators[]) {
    int n = 0;
    char *start = line;
    char *p = line;
    while (*p) {
        if (p[0] == '&' && p[1] == '&') {
            *p = '\0';
            p += 2;
            segments[n] = start;
            operators[n] = OP_AND;
            n++;
            start = p;
        } else if (p[0] == '|' && p[1] == '|') {
            *p = '\0';
            p += 2;
            segments[n] = start;
            operators[n] = OP_OR;
            n++;
            start = p;
        } else {
            p++;
        }
    }
    if (start && *start) {
        segments[n] = start;
        operators[n] = OP_NONE;
        n++;
    }
    return n;
}

/* 执行逻辑序列 */
static int execute_logical_sequence(char *line, const char *raw_cmd) {
    (void)raw_cmd;
    char *segments[MAX_SEQUENCE];
    int operators[MAX_SEQUENCE];
    int n = split_by_logic(line, segments, operators);

    for (int i = 0; i < n; i++) {
        if (is_blank(segments[i])) {
            fprintf(stderr, "oscdsh: 语法错误: 逻辑运算符前后缺少命令\n");
            return 1;
        }
    }

    int last_status = 0;
    for (int i = 0; i < n; i++) {
        if (is_blank(segments[i])) {
            if (i > 0 && operators[i-1] == OP_AND) {
                if (last_status != 0) continue;
            } else if (i > 0 && operators[i-1] == OP_OR) {
                if (last_status == 0) continue;
            }
            last_status = 0;
            continue;
        }

        int should_execute = 1;
        if (i > 0) {
            if (operators[i-1] == OP_AND) {
                should_execute = (last_status == 0);
            } else if (operators[i-1] == OP_OR) {
                should_execute = (last_status != 0);
            }
        }

        if (should_execute) {
            last_status = execute_pipeline_or_single(segments[i]);
        }
    }
    return 0;
}

/* 总入口 */
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

    // 1. 检测逻辑运算符 && 或 ||
    int has_logic = (strstr(line, "&&") != NULL || strstr(line, "||") != NULL);
    if (has_logic) {
        int len2 = strlen(line);
        while (len2 > 0 && isspace((unsigned char)line[len2-1])) {
            line[len2-1] = '\0';
            len2--;
        }
        if (len2 > 0 && line[len2-1] == '&') {
            fprintf(stderr, "oscdsh: 逻辑运算符不能与后台 '&' 同时使用\n");
            free(raw_cmd);
            return -1;
        }
        int ret = execute_logical_sequence(line, raw_cmd);
        free(raw_cmd);
        return ret;
    }

    // 2. 提取后台 &
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
        int fd_in = STDIN_FILENO, fd_out = STDOUT_FILENO;
        int ret = parse_single_cmd(cmds[0], args, &infile, &outfile, &append,
                                   &fd_in, &fd_out);
        if (ret < 0) {
            if (outfile) {
                int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
                int fd = open(outfile, flags, 0644);
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

        int result = execute_single(args, infile, outfile, append, fd_in, fd_out,
                                    background, raw_cmd);
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