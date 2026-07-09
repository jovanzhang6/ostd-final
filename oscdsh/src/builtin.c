// src/builtin.c
#include "oscdsh.h"

int builtin_hello(char **args) {
    (void)args;
    printf("Hello from OSCD Shell (oscdsh)!\n");
    return 0;
}

int builtin_exit(char **args) {
    (void)args;
    save_history();
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
    return 0;
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
            return 0;
        } else {
            printf("%s: not found\n", cmd);
            return 1;
        }
    }

    if (is_builtin_cmd(cmd)) {
        printf("%s is a shell builtin\n", cmd);
        return 0;
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
            return 0;
        }
        dir = strtok(NULL, ":");
    }

    printf("%s: not found\n", cmd);
    free(path_copy);
    return 1;
}

int builtin_history(char **args) {
    if (args[1] != NULL) {
        if (strcmp(args[1], "-c") == 0) {
            clear_history();
            return 0;
        }
        fprintf(stderr, "history: 无效选项 '%s'\n", args[1]);
        return 1;
    }
    print_history();
    return 0;
}

int builtin_alias(char **args) {
    if (args[1] == NULL) {
        print_aliases();
        return 0;
    }

    char *eq = NULL;
    int idx = 1;
    for (; args[idx] != NULL; idx++) {
        eq = strchr(args[idx], '=');
        if (eq) break;
    }

    if (eq == NULL) {
        const char *val = get_alias_value(args[1]);
        if (val == NULL) {
            printf("alias: %s: 未找到\n", args[1]);
            return 1;
        }
        printf("%s='%s'\n", args[1], val);
        return 0;
    }

    *eq = '\0';
    char *name = args[idx];
    char *value_start = eq + 1;

    char value[1024] = {0};
    strncpy(value, value_start, sizeof(value) - 1);

    for (int i = idx + 1; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0 ||
            strcmp(args[i], "<") == 0 || strcmp(args[i], "|") == 0) {
            break;
        }
        strcat(value, " ");
        strcat(value, args[i]);
    }

    size_t vlen = strlen(value);
    if (vlen >= 2 && value[0] == '\'' && value[vlen-1] == '\'') {
        memmove(value, value + 1, vlen - 1);
        value[vlen - 2] = '\0';
    }

    add_alias(name, value);
    return 0;
}

int builtin_unalias(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "unalias: 用法: unalias <名称>\n");
        return 1;
    }
    if (remove_alias(args[1]) != 0) {
        fprintf(stderr, "unalias: %s: 别名未找到\n", args[1]);
        return 1;
    }
    return 0;
}

int builtin_pwd(char **args) {
    (void)args;
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return 1;
    }
    printf("%s\n", cwd);
    return 0;
}

int builtin_export(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "export: 用法: export VAR=value 或 export VAR\n");
        return 1;
    }
    for (int i = 1; args[i] != NULL; i++) {
        char *eq = strchr(args[i], '=');
        if (eq) {
            *eq = '\0';
            char *name = args[i];
            char *value = eq + 1;
            if (setenv(name, value, 1) != 0) {
                perror("export");
                return 1;
            }
        } else {
            if (setenv(args[i], "", 1) != 0) {
                perror("export");
                return 1;
            }
        }
    }
    return 0;
}

int builtin_jobs(char **args) {
    (void)args;
    print_jobs();
    return 0;
}

int builtin_true(char **args) {
    (void)args;
    return 0;
}

int builtin_false(char **args) {
    (void)args;
    return 1;
}

/* 统一帮助函数 */
void builtin_help(const char *cmd) {
    if (strcmp(cmd, "cd") == 0) {
        printf("cd [目录]\n");
        printf("  无参数：切换到 HOME 目录。\n");
        printf("  -      ：切换到上一个工作目录。\n");
    } else if (strcmp(cmd, "type") == 0) {
        printf("type <命令>\n");
        printf("  判断命令是内置命令、外部程序或别名。\n");
    } else if (strcmp(cmd, "history") == 0) {
        printf("history [-c] [--help]\n");
        printf("  无参数      显示历史命令列表。\n");
        printf("  -c          清空历史记录。\n");
    } else if (strcmp(cmd, "alias") == 0) {
        printf("alias [名称[=值]]\n");
        printf("  无参数      显示所有别名。\n");
        printf("  名称        显示指定别名的值。\n");
        printf("  名称=值     定义新别名（值可用单引号括起）。\n");
    } else if (strcmp(cmd, "unalias") == 0) {
        printf("unalias <名称>\n");
        printf("  删除指定的别名。\n");
    } else if (strcmp(cmd, "pwd") == 0) {
        printf("pwd\n");
        printf("  打印当前工作目录的绝对路径。\n");
    } else if (strcmp(cmd, "export") == 0) {
        printf("export VAR=value ...\n");
        printf("  设置环境变量。\n");
    } else if (strcmp(cmd, "jobs") == 0) {
        printf("jobs\n");
        printf("  列出所有后台作业及其状态。\n");
    } else if (strcmp(cmd, "hello") == 0) {
        printf("hello\n");
        printf("  测试用，打印欢迎信息。\n");
    } else if (strcmp(cmd, "exit") == 0) {
        printf("exit\n");
        printf("  保存历史并退出 Shell。\n");
    } else if (strcmp(cmd, "true") == 0) {
        printf("true\n");
        printf("  返回成功（0）。通常用于逻辑测试。\n");
    } else if (strcmp(cmd, "false") == 0) {
        printf("false\n");
        printf("  返回失败（1）。通常用于逻辑测试。\n");
    } else {
        printf("%s: 未知的内置命令，无法提供帮助。\n", cmd);
    }
}