// src/completion.c
#include "oscdsh.h"

/* 内置命令列表（用于补全） */
static const char *builtins[] = {
    "hello", "exit", "cd", "type", "history",
    "alias", "unalias", "pwd", "export", "jobs", NULL
};

/* 命令名生成器：逐次返回匹配的内置命令和 PATH 中的可执行文件 */
static char *command_generator(const char *text, int state) {
    static int idx;                 // 静态变量，保持遍历状态
    static char **path_dirs = NULL; // PATH 分割目录列表
    static int path_idx;
    static DIR *cur_dir;
    static int builtin_done;        // 是否已输出完内置命令

    if (state == 0) {
        // 初始化
        idx = 0;
        path_idx = 0;
        builtin_done = 0;
        if (path_dirs) {
            for (int i = 0; path_dirs[i]; i++) free(path_dirs[i]);
            free(path_dirs);
            path_dirs = NULL;
        }
        if (cur_dir) { closedir(cur_dir); cur_dir = NULL; }

        // 分割 PATH 到数组
        char *path_env = getenv("PATH");
        if (path_env) {
            char *path_copy = strdup(path_env);
            if (path_copy) {
                int count = 0;
                char *tok = strtok(path_copy, ":");
                while (tok) count++, tok = strtok(NULL, ":");
                path_dirs = malloc((count + 1) * sizeof(char *));
                if (path_dirs) {
                    path_dirs[count] = NULL;
                    count = 0;
                    char *path_copy2 = strdup(path_env);
                    tok = strtok(path_copy2, ":");
                    while (tok) {
                        path_dirs[count++] = strdup(tok);
                        tok = strtok(NULL, ":");
                    }
                    free(path_copy2);
                }
                free(path_copy);
            }
        }
    }

    // 1. 返回匹配的内置命令
    if (!builtin_done) {
        while (builtins[idx]) {
            const char *name = builtins[idx++];
            if (strncmp(name, text, strlen(text)) == 0) {
                return strdup(name);
            }
        }
        builtin_done = 1;
    }

    // 2. 遍历 PATH 目录，返回匹配的可执行文件
    while (path_dirs && path_dirs[path_idx] != NULL) {
        if (!cur_dir) {
            cur_dir = opendir(path_dirs[path_idx]);
            if (!cur_dir) {
                path_idx++;
                continue;
            }
        }

        struct dirent *entry;
        while ((entry = readdir(cur_dir)) != NULL) {
            if (entry->d_name[0] == '.') continue; // 忽略隐藏文件
            if (strncmp(entry->d_name, text, strlen(text)) == 0) {
                // 简单检查可执行性：通过 access 判断（效率略低，但可行）
                char fullpath[1024];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", path_dirs[path_idx], entry->d_name);
                if (access(fullpath, X_OK) == 0) {
                    return strdup(entry->d_name);
                }
            }
        }
        closedir(cur_dir);
        cur_dir = NULL;
        path_idx++;
    }

    // 清理
    if (path_dirs) {
        for (int i = 0; path_dirs[i]; i++) free(path_dirs[i]);
        free(path_dirs);
        path_dirs = NULL;
    }
    return NULL;
}

/* 主补全函数 */
char **oscdsh_completion(const char *text, int start, int end) {
    (void)end;
    // 如果 text 包含 '/' 或以 '~'、'.' 开头，使用文件名补全
    if (text && (text[0] == '/' || text[0] == '.' || text[0] == '~' || strchr(text, '/'))) {
        return rl_completion_matches(text, rl_filename_completion_function);
    }
    // 命令补全：仅当 start == 0 时补全命令
    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }
    return NULL;
}