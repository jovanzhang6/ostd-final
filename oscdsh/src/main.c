// src/main.c
#include "oscdsh.h"

static volatile sig_atomic_t sig_received = 0;
int last_exit_code = 0;                /* 记录上一条命令的退出码，范围 0~255 */

static void signal_handler(int sig) {
    sig_received = sig;
}

static char *first_word(const char *cmd) {
    static char buf[MAX_CMD_LEN];
    strncpy(buf, cmd, MAX_CMD_LEN - 1);
    buf[MAX_CMD_LEN - 1] = '\0';
    return strtok(buf, " \t");
}

/* 环境变量展开：支持 $VAR ${VAR} $$ $? */
static char *expand_env(const char *line) {
    if (!line) return NULL;

    size_t len = strlen(line);
    char *result = malloc(MAX_CMD_LEN);
    if (!result) {
        perror("oscdsh: expand_env: malloc");
        return NULL;
    }

    size_t ri = 0;
    size_t i = 0;

    while (i < len && ri < MAX_CMD_LEN - 1) {
        if (line[i] == '$') {
            i++;
            if (i >= len) {
                result[ri++] = '$';
                break;
            }

            /* $$ -> PID */
            if (line[i] == '$') {
                char pidstr[32];
                snprintf(pidstr, sizeof(pidstr), "%d", getpid());
                size_t plen = strlen(pidstr);
                if (ri + plen < MAX_CMD_LEN) {
                    strcpy(result + ri, pidstr);
                    ri += plen;
                }
                i++;
                continue;
            }

            /* $? -> 上一条命令退出码 */
            if (line[i] == '?') {
                char exitstr[32];
                snprintf(exitstr, sizeof(exitstr), "%d", last_exit_code);
                size_t elen = strlen(exitstr);
                if (ri + elen < MAX_CMD_LEN) {
                    strcpy(result + ri, exitstr);
                    ri += elen;
                }
                i++;
                continue;
            }

            /* ${VAR} */
            if (line[i] == '{') {
                i++;
                const char *start = line + i;
                const char *end = strchr(start, '}');
                if (end) {
                    size_t vlen = end - start;
                    char varname[128];
                    if (vlen >= sizeof(varname)) vlen = sizeof(varname) - 1;
                    memcpy(varname, start, vlen);
                    varname[vlen] = '\0';
                    const char *val = getenv(varname);
                    if (val) {
                        size_t vallen = strlen(val);
                        if (ri + vallen < MAX_CMD_LEN) {
                            strcpy(result + ri, val);
                            ri += vallen;
                        }
                    }
                    i = end - line + 1;
                } else {
                    result[ri++] = '$';
                    result[ri++] = '{';
                }
                continue;
            }

            /* $VAR */
            const char *var_start = line + i;
            (void)var_start;
            size_t j = i;
            while (j < len && (isalnum((unsigned char)line[j]) || line[j] == '_'))
                j++;
            size_t vlen = j - i;
            if (vlen > 0) {
                char varname[128];
                if (vlen >= sizeof(varname)) vlen = sizeof(varname) - 1;
                memcpy(varname, line + i, vlen);
                varname[vlen] = '\0';
                const char *val = getenv(varname);
                if (val) {
                    size_t vallen = strlen(val);
                    if (ri + vallen < MAX_CMD_LEN) {
                        strcpy(result + ri, val);
                        ri += vallen;
                    }
                }
                i = j;
            } else {
                result[ri++] = '$';
            }
        } else {
            result[ri++] = line[i++];
        }
    }

    result[ri] = '\0';
    return result;
}

int main() {
    init_jobs();
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, SIG_IGN);
    rl_attempted_completion_function = oscdsh_completion;

    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    load_history();

    {
        char *shlvl_str = getenv("SHLVL");
        int shlvl = 1;
        if (shlvl_str) {
            shlvl = atoi(shlvl_str) + 1;
            if (shlvl < 1) shlvl = 1;
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", shlvl);
        setenv("SHLVL", buf, 1);
    }

    /* 设置 SHELL 变量为当前可执行文件路径 */
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len != -1) {
        exe_path[len] = '\0';
        setenv("SHELL", exe_path, 1);
    }

    // 设置PID和PPID
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", getpid());
    setenv("PID", buf, 1);
    snprintf(buf, sizeof(buf), "%d", getppid());
    setenv("PPID", buf, 1);

    printf("\033[1;36m");  // 亮青色
    printf("╔══════════════════════════════════════════╗\n");
    printf("║                                          ║\n");
    printf("║      \033[1;33m欢迎使用 OSCD Shell (oscdsh)\033[1;36m        ║\n");
    printf("║                                          ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  嵌套层级 : %-29s║\n", getenv("SHLVL"));
    printf("║  Tab 补全 | --help 查看帮助              ║\n");
    printf("║  输入 exit 或 Ctrl+D 退出                ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("\033[0m\n");     // 重置颜色

    while (1) {
        char *prompt = get_prompt();
        char *input = readline(prompt);
        free(prompt);

        if (sig_received) {
            printf("\noscdsh: 收到信号 %d，正在保存历史并退出...\n", sig_received);
            save_history();
            free(input);
            break;
        }

        if (!input) {
            if (errno == EINTR) {
                continue;
            }
            printf("\n");
            save_history();
            break;
        }

        char *trimmed = input;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
        if (*trimmed == '\0') {
            free(input);
            continue;
        }

        char line[MAX_CMD_LEN];
        if (strlen(trimmed) >= MAX_CMD_LEN) {
            fprintf(stderr, "oscdsh: 输入超过最大长度 %d，已截断\n", MAX_CMD_LEN);
        }
        strncpy(line, trimmed, MAX_CMD_LEN - 1);
        line[MAX_CMD_LEN - 1] = '\0';

        /* 历史展开 */
        if (line[0] == '!') {
            char *expanded = expand_history(line);
            if (expanded == NULL) {
                free(input);
                continue;
            }
            printf("%s\n", expanded);
            strncpy(line, expanded, MAX_CMD_LEN - 1);
            line[MAX_CMD_LEN - 1] = '\0';
            free(expanded);
        }

        /* 别名展开 */
        {
            int max_expand = 10;
            while (max_expand-- > 0) {
                char *first = first_word(line);
                if (first == NULL) break;
                const char *alias_val = get_alias_value(first);
                if (alias_val == NULL) break;

                char *rest = line + strlen(first);
                while (*rest == ' ' || *rest == '\t') rest++;

                char new_line[MAX_CMD_LEN];
                snprintf(new_line, sizeof(new_line), "%s %s", alias_val, rest);
                strncpy(line, new_line, MAX_CMD_LEN - 1);
                line[MAX_CMD_LEN - 1] = '\0';
            }
            if (max_expand <= 0) {
                fprintf(stderr, "oscdsh: 别名递归层次太深\n");
                free(input);
                continue;
            }
        }

        /* 环境变量展开 */
        {
            char *expanded_line = expand_env(line);
            if (expanded_line) {
                strncpy(line, expanded_line, MAX_CMD_LEN - 1);
                line[MAX_CMD_LEN - 1] = '\0';
                free(expanded_line);
            }
        }

        // 将所有命令同时加入 Readline 历史与自定义历史（完全同步）
        add_history(line);
        hist_add(line);

        /* 执行命令并记录退出码，保证 $? 范围为 0~255 */
        {
            int raw_ret = execute_command(line);
            if (raw_ret < 0) raw_ret = 1;        // 语法错误等视作一般失败
            last_exit_code = raw_ret & 0xFF;     // 取低 8 位，确保合法范围
        }

        free(input);

        if (sig_received) {
            printf("\noscdsh: 收到信号 %d，正在保存历史并退出...\n", sig_received);
            save_history();
            break;
        }
    }

    return 0;
}