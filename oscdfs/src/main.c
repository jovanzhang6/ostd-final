// oscdfs/src/main.c
#include "oscdfs.h"

/* 颜色与样式宏 (仅用于交互终端) */
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_CYAN    "\x1b[36m"
#define STYLE_BOLD    "\x1b[1m"

static void print_prompt(void)
{
    int use_color = isatty(STDIN_FILENO);
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[9];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

    if (use_color) {
        const char *user_color = (current_uid == 0) ? COLOR_RED : COLOR_GREEN;
        printf("%s[%s]%s %s%s%s@oscdfs %s%s%s $ ",
               COLOR_CYAN, time_str, COLOR_RESET,
               STYLE_BOLD, user_color, get_current_username(),
               COLOR_BLUE, cwd_path,
               COLOR_RESET);
    } else {
        printf("[%s] %s@oscdfs %s $ ", time_str, get_current_username(), cwd_path);
    }
    fflush(stdout);
}

static void print_usage(const char *prog)
{
    fprintf(stderr, "用法: %s [--init] [-f <disk_path>] [-m <mountpoint>] [FUSE 选项...]\n", prog);
    fprintf(stderr, "      若不指定 -f，默认使用当前目录下的 disk.img，\n");
    fprintf(stderr, "      也可设置环境变量 OSCDISK 来指定默认磁盘文件。\n");
}

int main(int argc, char *argv[])
{
    const char *disk_path = "disk.img";
    int do_init = 0;
    int do_fuse = 0;
    char *mountpoint = NULL;

    /* 解析自己的选项，保留其余参数给 FUSE */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--init") == 0) {
            do_init = 1;
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--disk") == 0) {
            if (i + 1 < argc) {
                disk_path = argv[++i];
            } else {
                fprintf(stderr, "oscdfs: -f 需要一个参数\n");
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mount") == 0) {
            if (i + 1 < argc) {
                mountpoint = argv[++i];
                do_fuse = 1;
            } else {
                fprintf(stderr, "oscdfs: -m 需要一个参数\n");
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            /* 如果是 FUSE 模式，其他参数可能是 FUSE 自身的参数，稍后收集 */
            if (do_fuse) continue;
            fprintf(stderr, "oscdfs: 未知选项 '%s'\n", argv[i]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* 环境变量 OSCDISK */
    char *env = getenv("OSCDISK");
    if (env != NULL && strcmp(disk_path, "disk.img") == 0)
        disk_path = env;

    if (do_init) {
        if (mkfs_disk(disk_path) != 0) {
            fprintf(stderr, "oscdfs: --init failed\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    if (disk_open(disk_path) != 0) {
        fprintf(stderr, "oscdfs: failed to open '%s', "
                "run '%s --init [-f <disk>]' first.\n", disk_path, argv[0]);
        return EXIT_FAILURE;
    }

    if (do_fuse) {
        /* 构建 FUSE 参数：程序名 + 其他选项 + mountpoint */
        int fuse_argc = 0;
        char *fuse_argv[32];
        fuse_argv[fuse_argc++] = argv[0];
        fuse_argv[fuse_argc++] = "-s";   /* 单线程更安全 */

        for (int i = 1; i < argc; i++) {
            /* 跳过我们自己解析过的选项 */
            if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--disk") == 0) {
                i++; continue;
            }
            if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mount") == 0) {
                i++; continue;
            }
            if (strcmp(argv[i], "--init") == 0) continue;
            fuse_argv[fuse_argc++] = argv[i];
        }
        fuse_argv[fuse_argc++] = mountpoint;
        fuse_argv[fuse_argc] = NULL;

        current_uid = 0;
        current_gid = 0;
        int ret = oscdfs_fuse_main(fuse_argc, fuse_argv);
        disk_close();
        return ret;
    }

    /* 交互模式 */
    if (oscdfs_login("root", "root") != 0) {
        fprintf(stderr, "oscdfs: auto login failed, bad disk?\n");
        disk_close();
        return EXIT_FAILURE;
    }

    /* 启动画面 */
    printf("\n"
           "   ██████╗ ███████╗ ██████╗██████╗ ███████╗███████╗\n"
           "  ██╔═══██╗██╔════╝██╔════╝██╔══██╗██╔════╝██╔════╝\n"
           "  ██║   ██║███████╗██║     ██║  ██║█████╗  ███████╗\n"
           "  ██║   ██║╚════██║██║     ██║  ██║██╔══╝  ╚════██║\n"
           "  ╚██████╔╝███████║╚██████╗██████╔╝██║     ███████║\n"
           "   ╚═════╝ ╚══════╝ ╚═════╝╚═════╝ ╚═╝     ╚══════╝\n"
           "     OS Course Design File System\n");
    printf("  磁盘文件: %s\n\n", disk_path);

    char line[MAX_CMD_LEN];
    while (1) {
        print_prompt();

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