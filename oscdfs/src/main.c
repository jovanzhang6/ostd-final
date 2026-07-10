// oscdfs/src/main.c
#include "oscdfs.h"

int main(int argc, char *argv[])
{
    /* 处理 --init 参数 */
    if (argc == 2 && strcmp(argv[1], "--init") == 0) {
        if (mkfs_disk("disk.img") != 0) {
            fprintf(stderr, "oscdfs: --init failed\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    /* 原有的交互模式 */
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

        line[strcspn(line, "\n")] = '\0';

        /* 处理超长行 */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] != '\n') {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }

        if (line[0] == '\0')
            continue;

        execute_command(line);
    }

    return EXIT_SUCCESS;
}