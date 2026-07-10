// oscdfs/src/main.c
#include "oscdfs.h"

int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "--init") == 0) {
        if (mkfs_disk("disk.img") != 0) {
            fprintf(stderr, "oscdfs: --init failed\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    if (disk_open("disk.img") != 0) {
        fprintf(stderr, "oscdfs: failed to open disk.img, "
                "run './oscdfs --init' first.\n");
        return EXIT_FAILURE;
    }

    /* 自动以 root 登录，设置 uid/gid 和家目录 */
    if (oscdfs_login("root", "root") != 0) {
        fprintf(stderr, "oscdfs: auto login failed, bad disk?\n");
        disk_close();
        return EXIT_FAILURE;
    }

    char line[MAX_CMD_LEN];
    printf("OSCD File System (oscdfs)\n");
    printf("输入 'dir' 查看当前目录, 'exit' 退出\n\n");

    while (1) {
        printf(PROMPT);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        size_t len = strlen(line);
        int has_newline = 0;
        if (len > 0 && line[len-1] == '\n') {
            has_newline = 1;
            line[--len] = '\0';
        }
        if (len > 0 && line[len-1] == '\r') {
            line[--len] = '\0';
        }

        if (!has_newline) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }

        if (line[0] == '\0')
            continue;

        execute_command(line);
    }

    disk_close();
    return EXIT_SUCCESS;
}