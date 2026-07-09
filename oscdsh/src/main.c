#include "oscdsh.h"

int main() {
    char line[MAX_CMD_LEN];

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

        execute_command(line);
    }

    return 0;
}