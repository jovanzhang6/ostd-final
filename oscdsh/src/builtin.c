// src/builtin.c
#include "oscdsh.h"

int builtin_hello(char **args) {
    (void)args;
    printf("Hello from OSCD Shell (oscdsh)!\n");
    return 1;
}

int builtin_exit(char **args) {
    (void)args;
    printf("Bye from oscdsh!\n");
    exit(0);
}

int builtin_cd(char **args) {
    char *target = NULL;
    int show_path = 0;

    if (args[1] == NULL) {
        target = getenv("HOME");
        if (target == NULL) target = "/";
    } else if (strcmp(args[1], "-") == 0) {
        target = getenv("OLDPWD");
        if (target == NULL) {
            fprintf(stderr, "oscdsh: cd: OLDPWD 未设置\n");
            return 1;
        }
        show_path = 1;
    } else {
        target = args[1];
    }

    char old_cwd[1024];
    if (getcwd(old_cwd, sizeof(old_cwd)) == NULL) {
        perror("oscdsh: cd: getcwd");
        return 1;
    }

    if (chdir(target) != 0) {
        perror("oscdsh: cd");
        return 1;
    }

    setenv("OLDPWD", old_cwd, 1);
    char new_cwd[1024];
    if (getcwd(new_cwd, sizeof(new_cwd)) != NULL) {
        setenv("PWD", new_cwd, 1);
    }

    if (show_path) {
        printf("%s\n", getenv("PWD") ? getenv("PWD") : target);
    }

    return 1;
}

int builtin_type(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "type: 用法: type <命令>\n");
        return 1;
    }
    const char *cmd = args[1];

    if (strchr(cmd, '/')) {
        if (access(cmd, X_OK) == 0) {
            printf("%s is %s\n", cmd, cmd);
        } else {
            printf("%s: not found\n", cmd);
        }
        return 1;
    }

    if (is_builtin_cmd(cmd)) {
        printf("%s is a shell builtin\n", cmd);
        return 1;
    }

    char *path_env = getenv("PATH");
    if (path_env == NULL) {
        printf("%s: not found\n", cmd);
        return 1;
    }

    char *path_copy = strdup(path_env);
    if (path_copy == NULL) {
        perror("type: strdup");
        return 1;
    }

    char *dir = strtok(path_copy, ":");
    while (dir) {
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, cmd);
        if (access(fullpath, X_OK) == 0) {
            printf("%s is %s\n", cmd, fullpath);
            free(path_copy);
            return 1;
        }
        dir = strtok(NULL, ":");
    }

    printf("%s: not found\n", cmd);
    free(path_copy);
    return 1;
}

int builtin_history(char **args) {
    (void)args;
    print_history();
    return 1;
}

int builtin_alias(char **args) {
    if (args[1] == NULL) {
        print_aliases();
        return 1;
    }

    // 查找含有 '=' 的 token
    char *eq = NULL;
    int idx = 1;
    for (; args[idx] != NULL; idx++) {
        eq = strchr(args[idx], '=');
        if (eq) break;
    }

    if (eq == NULL) {
        // 没有等号，显示单个别名
        print_one_alias(args[1]);
        return 1;
    }

    // 提取名称和值的起始部分
    *eq = '\0';
    char *name = args[idx];
    char *value_start = eq + 1;

    // 拼接完整值：从当前 token 剩余部分 + 后续 token，用空格连接
    char value[1024] = {0};
    strncpy(value, value_start, sizeof(value) - 1);

    for (int i = idx + 1; args[i] != NULL; i++) {
        // 遇到重定向或管道符则停止（别名值不应包含这些）
        if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0 ||
            strcmp(args[i], "<") == 0 || strcmp(args[i], "|") == 0) {
            break;
        }
        strcat(value, " ");
        strcat(value, args[i]);
    }

    // 去除首尾单引号（若存在）
    size_t vlen = strlen(value);
    if (vlen >= 2 && value[0] == '\'' && value[vlen-1] == '\'') {
        memmove(value, value + 1, vlen - 1);
        value[vlen - 2] = '\0';
    }

    add_alias(name, value);
    return 1;
}

int builtin_unalias(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "unalias: 用法: unalias <名称>\n");
        return 1;
    }
    if (remove_alias(args[1]) != 0) {
        fprintf(stderr, "unalias: %s: 别名未找到\n", args[1]);
    }
    return 1;
}