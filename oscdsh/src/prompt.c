// src/prompt.c
#include "oscdsh.h"
#include <time.h>

#define PROMPT_SIZE 2048

/* ANSI 颜色定义 */
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_BGREEN  "\033[1;32m"   /* 亮绿加粗 */
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"
#define COLOR_BRED    "\033[1;31m"   /* 红加粗 */

char *get_prompt(void) {
    char user[128] = "unknown";
    char host[128] = "unknown";
    char cwd[1024];
    char dir[1024];
    char time_str[32];

    // 用户名
    const char *env_user = getenv("USER");
    if (env_user) {
        strncpy(user, env_user, sizeof(user) - 1);
        user[sizeof(user) - 1] = '\0';
    }

    // 主机名
    if (gethostname(host, sizeof(host)) == 0) {
        char *dot = strchr(host, '.');
        if (dot) *dot = '\0';
    }

    // 当前时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    } else {
        strcpy(time_str, "??:??:??");
    }

    // 当前工作目录
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(dir, "?");
    } else {
        const char *home = getenv("HOME");
        if (home && strncmp(cwd, home, strlen(home)) == 0) {
            size_t home_len = strlen(home);
            if (cwd[home_len] == '\0') {
                strcpy(dir, "~");
            } else {
                snprintf(dir, sizeof(dir), "~%s", cwd + home_len);
            }
        } else {
            strncpy(dir, cwd, sizeof(dir) - 1);
            dir[sizeof(dir) - 1] = '\0';
        }
    }

    // 构建着色提示符
    char buf[PROMPT_SIZE];
    const char *user_color  = COLOR_BGREEN;          /* 用户名：亮绿加粗 */
    const char *host_color  = COLOR_BLUE;            /* 主机名：蓝色 */
    const char *time_color  = COLOR_CYAN;            /* 时间：青色 */
    const char *dir_color   = COLOR_YELLOW;          /* 目录：黄色 */
    const char *end_color;                           /* 提示符结尾符号颜色 */
    char end_char = (getuid() == 0) ? '#' : '$';

    if (getuid() == 0) {
        end_color = COLOR_BRED;                      /* root：红加粗 # */
    } else {
        end_color = COLOR_GREEN;                     /* 普通用户：绿色 $ */
    }

    // 格式: [user@host HH:MM:SS dir]$
    snprintf(buf, sizeof(buf),
             "[" COLOR_RESET              /* 开启默认色 */
             "%s%s" COLOR_RESET           /* 用户名 */
             "@" COLOR_RESET
             "%s%s" COLOR_RESET           /* 主机名 */
             " " COLOR_RESET
             "%s%s" COLOR_RESET           /* 时间 */
             " " COLOR_RESET
             "%s%s" COLOR_RESET           /* 目录 */
             "]" COLOR_RESET
             "%s%c" COLOR_RESET " ",      /* 提示符 */
             user_color,   user,
             host_color,   host,
             time_color,   time_str,
             dir_color,    dir,
             end_color,    end_char);
    return strdup(buf);
}