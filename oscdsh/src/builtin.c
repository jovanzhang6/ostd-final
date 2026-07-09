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