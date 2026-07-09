// src/main.c
#include "oscdsh.h"
#include "jobs.h"
#include <signal.h>
#include <string.h>

/* 提取命令行第一个单词，返回静态缓冲区 */
static char *first_word(const char *cmd) {
    static char buf[MAX_CMD_LEN];
    strncpy(buf, cmd, MAX_CMD_LEN - 1);
    buf[MAX_CMD_LEN - 1] = '\0';
    return strtok(buf, " \t");
}

int main() {
    char line[MAX_CMD_LEN];

    init_jobs();
    signal(SIGCHLD, sigchld_handler);

    printf("Operating System Course Design Shell\n");
    printf("输入 'hello' 测试，输入 'exit' 退出\n\n");

    while (1) {
        printf(PROMPT);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0') continue;

        // 历史展开（!）
        if (line[0] == '!') {
            char *expanded = expand_history(line);
            if (expanded == NULL) {
                continue;
            }
            printf("%s\n", expanded);
            strncpy(line, expanded, MAX_CMD_LEN - 1);
            line[MAX_CMD_LEN - 1] = '\0';
            free(expanded);
        }

        // 别名展开（循环，防止递归）
        {
            int max_expand = 10;
            while (max_expand-- > 0) {
                char *first = first_word(line);
                if (first == NULL) break;
                const char *alias_val = get_alias_value(first);
                if (alias_val == NULL) break;

                // 找到第一个单词后的剩余部分
                char *rest = line + strlen(first);
                while (*rest == ' ' || *rest == '\t') rest++;

                char new_line[MAX_CMD_LEN];
                snprintf(new_line, sizeof(new_line), "%s %s", alias_val, rest);
                strncpy(line, new_line, MAX_CMD_LEN - 1);
                line[MAX_CMD_LEN - 1] = '\0';
            }
            if (max_expand <= 0) {
                fprintf(stderr, "oscdsh: 别名递归层次太深\n");
                continue;
            }
        }

        // 添加历史（跳过 history 本身）
        {
            char *first = first_word(line);
            if (first == NULL || strcmp(first, "history") != 0) {
                add_history(line);
            }
        }

        // 执行命令
        execute_command(line);
    }

    return 0;
}