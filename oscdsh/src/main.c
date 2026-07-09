// src/main.c
#include "oscdsh.h"
#include "jobs.h"
#include <signal.h>
#include <string.h>

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

        // 历史展开
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

        // 添加历史（跳过 history 命令本身）
        {
            char *check_cmd = strdup(line);
            if (check_cmd) {
                char *first_word = strtok(check_cmd, " \t");
                if (first_word == NULL || strcmp(first_word, "history") != 0) {
                    add_history(line);
                }
                free(check_cmd);
            }
        }

        // 执行命令
        execute_command(line);
    }

    return 0;
}