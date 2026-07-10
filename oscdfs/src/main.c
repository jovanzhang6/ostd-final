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

    current_dir_inode = 1; /* 根目录 inode 设为 1 */
    current_uid = 0;
    current_gid = 0;
    memset(fd_table, 0, sizeof(fd_table));

    char line[MAX_CMD_LEN];
    printf("OSCD File System (oscdfs)\n");
    printf("输入 'dir' 查看根目录, 'exit' 退出\n\n");

    while (1) {
        printf(PROMPT);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        /* ----- 修正后的行尾处理与超长检测 ----- */
        size_t len = strlen(line);
        int has_newline = 0;

        /* 检查末尾是否有换行符 */
        if (len > 0 && line[len-1] == '\n') {
            has_newline = 1;
            line[--len] = '\0';          /* 去除 '\n' */
        }
        /* 若换行前是回车，也去掉 */
        if (len > 0 && line[len-1] == '\r') {
            line[--len] = '\0';
        }

        /* 如果没有遇到换行，说明输入超长，清空 stdin 剩余内容 */
        if (!has_newline) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            /* 可选：提示用户命令过长 */
            fprintf(stderr, "oscdfs: 警告：输入过长，已截断\n");
        }
        /* ------------------------------------- */

        if (line[0] == '\0')
            continue;

        execute_command(line);
    }

    disk_close();
    return EXIT_SUCCESS;
}