// src/main.c
#include "oscdsh.h"

static volatile sig_atomic_t sig_received = 0;

static void signal_handler(int sig) {
    sig_received = sig;
}

static char *first_word(const char *cmd) {
    static char buf[MAX_CMD_LEN];
    strncpy(buf, cmd, MAX_CMD_LEN - 1);
    buf[MAX_CMD_LEN - 1] = '\0';
    return strtok(buf, " \t");
}

int main() {
    init_jobs();
    signal(SIGCHLD, sigchld_handler);
    rl_attempted_completion_function = oscdsh_completion;

    /* 注册信号处理，用于强制退出时尽可能保存历史 */
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    load_history();

    printf("Operating System Course Design Shell\n");
    printf("输入 'hello' 测试，输入 'exit' 退出，Tab 可补全\n\n");

    while (1) {
        char *prompt = get_prompt();
        char *input = readline(prompt);
        free(prompt);

        /* 优先检查是否收到退出信号 */
        if (sig_received) {
            printf("\noscdsh: 收到信号 %d，正在保存历史并退出...\n", sig_received);
            save_history();
            free(input);
            break;
        }

        if (!input) {               /* EOF 或中断 */
            if (errno == EINTR) {
                continue;           /* 被信号中断，重新读取 */
            }
            printf("\n");
            save_history();
            break;
        }

        /* 去除前导空白 */
        char *trimmed = input;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
        if (*trimmed == '\0') {
            free(input);
            continue;
        }

        /* 复制到可修改的缓冲区 */
        char line[MAX_CMD_LEN];
        strncpy(line, trimmed, MAX_CMD_LEN - 1);
        line[MAX_CMD_LEN - 1] = '\0';

        /* 历史展开（以 ! 开头） */
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

        /* 别名展开（防止递归深度过大） */
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

        /* 同时添加到 Readline 历史（上下键浏览） */
        add_history(line);

        /* 添加到自定义历史（history 命令、! 展开），过滤掉 history 和 jobs 本身 */
        {
            char *first = first_word(line);
            if (first == NULL || (strcmp(first, "history") != 0 && strcmp(first, "jobs") != 0)) {
                hist_add(line);
            }
        }

        /* 执行命令 */
        execute_command(line);
        free(input);

        /* 执行完命令后再次检查信号 */
        if (sig_received) {
            printf("\noscdsh: 收到信号 %d，正在保存历史并退出...\n", sig_received);
            save_history();
            break;
        }
    }

    return 0;
}