// src/completion.c
#include "oscdsh.h"

/* 内置命令列表（用于补全） */
static const char *builtins[] = {
    "hello", "exit", "cd", "type", "history",
    "alias", "unalias", "pwd", "export", "jobs",
    "true", "false", NULL
};

/* 补全生成器的静态状态（提升到文件作用域，防止泄漏） */
static char **gen_path_dirs = NULL;
static int    gen_path_idx = 0;
static DIR   *gen_cur_dir = NULL;
static int    gen_builtin_done = 0;

/* 清理生成器占用的资源 */
static void cleanup_generator(void) {
    if (gen_path_dirs) {
        for (int i = 0; gen_path_dirs[i]; i++)
            free(gen_path_dirs[i]);
        free(gen_path_dirs);
        gen_path_dirs = NULL;
    }
    if (gen_cur_dir) {
        closedir(gen_cur_dir);
        gen_cur_dir = NULL;
    }
    gen_builtin_done = 0;
    gen_path_idx = 0;
}

/* 命令名生成器：逐次返回匹配的内置命令和 PATH 中的可执行文件 */
static char *command_generator(const char *text, int state) {
    static int idx;

    if (state == 0) {
        // 重置并初始化
        idx = 0;
        gen_path_idx = 0;
        gen_builtin_done = 0;
        cleanup_generator();          // 先释放可能残留的资源

        // 分割 PATH 到数组
        char *path_env = getenv("PATH");
        if (path_env) {
            char *path_copy = strdup(path_env);
            if (path_copy) {
                int count = 0;
                char *tok = strtok(path_copy, ":");
                while (tok) count++, tok = strtok(NULL, ":");
                gen_path_dirs = malloc((count + 1) * sizeof(char *));
                if (gen_path_dirs) {
                    gen_path_dirs[count] = NULL;
                    count = 0;
                    char *path_copy2 = strdup(path_env);
                    tok = strtok(path_copy2, ":");
                    while (tok) {
                        gen_path_dirs[count++] = strdup(tok);
                        tok = strtok(NULL, ":");
                    }
                    free(path_copy2);
                }
                free(path_copy);
            }
        }
    }

    // 1. 返回匹配的内置命令
    if (!gen_builtin_done) {
        while (builtins[idx]) {
            const char *name = builtins[idx++];
            if (strncmp(name, text, strlen(text)) == 0) {
                char *dup = strdup(name);
                if (!dup) perror("oscdsh: command_generator: strdup");
                return dup;
            }
        }
        gen_builtin_done = 1;
    }

    // 2. 遍历 PATH 目录，返回匹配的可执行文件
    while (gen_path_dirs && gen_path_dirs[gen_path_idx] != NULL) {
        if (!gen_cur_dir) {
            gen_cur_dir = opendir(gen_path_dirs[gen_path_idx]);
            if (!gen_cur_dir) {
                gen_path_idx++;
                continue;
            }
        }

        struct dirent *entry;
        while ((entry = readdir(gen_cur_dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (strncmp(entry->d_name, text, strlen(text)) == 0) {
                char fullpath[1024];
                snprintf(fullpath, sizeof(fullpath), "%s/%s",
                         gen_path_dirs[gen_path_idx], entry->d_name);
                if (access(fullpath, X_OK) == 0) {
                    char *dup = strdup(entry->d_name);
                    if (!dup) perror("oscdsh: command_generator: strdup");
                    return dup;
                }
            }
        }
        closedir(gen_cur_dir);
        gen_cur_dir = NULL;
        gen_path_idx++;
    }

    // 全部遍历完毕，彻底清理
    cleanup_generator();
    return NULL;
}

/* 主补全函数 */
char **oscdsh_completion(const char *text, int start, int end) {
    (void)end;
    // 每次补全入口先强制清理生成器，避免因中断导致的残留
    cleanup_generator();

    // 如果 text 包含 '/' 或以 '~'、'.'、'/' 开头，使用文件名补全
    if (text && (text[0] == '/' || text[0] == '.' || text[0] == '~' || strchr(text, '/'))) {
        return rl_completion_matches(text, rl_filename_completion_function);
    }
    // 命令补全：仅当 start == 0 时补全命令
    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }
    // 非行首且不是路径，默认文件名补全（补全参数中的文件名）
    return rl_completion_matches(text, rl_filename_completion_function);
}