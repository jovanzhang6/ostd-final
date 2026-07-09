// src/main.c
#include "oscdsh.h"
#include "jobs.h"
#include <signal.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

/* 提取命令行第一个单词 */
static char *first_word(const char *cmd) {
    static char buf[MAX_CMD_LEN];
    strncpy(buf, cmd, MAX_CMD_LEN - 1);
    buf[MAX_CMD_LEN - 1] = '\0';
    return strtok(buf, " \t");
}

int main() {
    init_jobs();
    signal(SIGCHLD, sigchld_handler);

    /* 设置 Readline 补全函数 */
    rl_attempted_completion_function = oscdsh_completion;

    printf("Operating System Course Design Shell\n");
    printf("输入 'hello' 测试，输入 'exit' 退出，Tab 可补全\n\n");

    while (1) {
        char *input = readline(PROMPT);
        if (!input) {          // Ctrl-D
            printf("\n");
            break;
        }

        // 去除首尾空白（readline 返回的通常没有前导空白，但可能有尾部空白）
        char *trimmed = input;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
        if (*trimmed == '\0') {
            free(input);
            continue;          // 空行跳过
        }

        // 复制到 line 缓冲区（方便历史展开等操作修改字符串）
        char line[MAX_CMD_LEN];
        strncpy(line, trimmed, MAX_CMD_LEN - 1);
        line[MAX_CMD_LEN - 1] = '\0';

        // 历史展开（!）
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

        // 别名展开（循环）
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

        // 将最终命令添加到 readline 的内置历史（用于上下箭头浏览）
        add_history(line);    // Readline 的 add_history

        // 添加我们自己的历史（用于 history 命令和 ! 展开）
        {
            char *first = first_word(line);
            if (first == NULL || (strcmp(first, "history") != 0 && strcmp(first, "jobs") != 0)) {
                hist_add(line);
            }
        }

        // 执行命令
        execute_command(line);

        free(input);
    }

    return 0;
}