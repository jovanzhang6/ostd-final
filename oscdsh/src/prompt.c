// src/prompt.c
#include "oscdsh.h"

#define PROMPT_SIZE 2048

char *get_prompt(void) {
    char user[128] = "unknown";
    char host[128] = "unknown";
    char cwd[1024];
    char dir[1024];

    const char *env_user = getenv("USER");
    if (env_user) {
        strncpy(user, env_user, sizeof(user) - 1);
        user[sizeof(user) - 1] = '\0';
    }

    if (gethostname(host, sizeof(host)) == 0) {
        char *dot = strchr(host, '.');
        if (dot) *dot = '\0';
    }

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

    char buf[PROMPT_SIZE];
    snprintf(buf, sizeof(buf), "[%s@%s %s]$ ", user, host, dir);
    return strdup(buf);
}