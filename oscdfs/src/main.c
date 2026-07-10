// oscdfs/src/main.c
#include "oscdfs.h"

int main(void) {
    char line[MAX_CMD_LEN];

    printf("OSCD File System (oscdfs)\n");
    printf("输入 'hello' 测试，输入 'exit' 退出\n\n");

    while (1) {
        printf(PROMPT);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        /* 去掉尾部的换行符 */
        line[strcspn(line, "\n")] = '\0';

        /* 如果当前行尾部没有换行符，说明输入行超长，清空剩余输入 */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] != '\n') {
            /* 清除stdin中本行剩余的字符 */
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }

        if (line[0] == '\0')
            continue;

        execute_command(line);
    }

    return 0;
}