// src/history.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "oscdsh.h"

static char *osh_history_list[MAX_HISTORY];
int history_count = 0;

void hist_add(const char *cmd) {
    if (cmd == NULL || *cmd == '\0') return;
    if (history_count >= MAX_HISTORY) {
        fprintf(stderr, "oscdsh: 历史记录已满，无法记录新命令\n");
        return;
    }
    osh_history_list[history_count] = strdup(cmd);
    if (osh_history_list[history_count] == NULL) {
        perror("oscdsh: strdup");
        return;
    }
    history_count++;
}

static const char *get_history(int index) {
    if (index < 1 || index > history_count) return NULL;
    return osh_history_list[index - 1];
}

static const char *reverse_search_history(const char *substr) {
    for (int i = history_count - 1; i >= 0; i--) {
        if (strstr(osh_history_list[i], substr) != NULL) {
            return osh_history_list[i];
        }
    }
    return NULL;
}

char *expand_history(const char *input) {
    if (input == NULL || input[0] != '!') return NULL;

    char *work = strdup(input);
    if (work == NULL) {
        perror("oscdsh: strdup");
        return NULL;
    }

    char *pound = strchr(work, '#');
    if (pound) *pound = '\0';

    int len = strlen(work);
    while (len > 0 && isspace((unsigned char)work[len-1])) work[--len] = '\0';

    if (len == 1) {
        free(work);
        fprintf(stderr, "oscdsh: !: 格式错误\n");
        return NULL;
    }

    if (strcmp(work, "!!") == 0) {
        free(work);
        if (history_count == 0) {
            fprintf(stderr, "oscdsh: !: 无历史命令\n");
            return NULL;
        }
        return strdup(osh_history_list[history_count - 1]);
    }

    const char *p = work + 1;
    if (*p >= '0' && *p <= '9') {
        int n = atoi(p);
        free(work);
        const char *cmd = get_history(n);
        if (cmd == NULL) {
            fprintf(stderr, "oscdsh: !%d: 无此历史编号\n", n);
            return NULL;
        }
        return strdup(cmd);
    }

    if (*p == '?') {
        const char *start = p + 1;
        const char *end = strchr(start, '?');
        size_t len_sub;
        char substr[MAX_CMD_LEN];
        if (end != NULL) {
            len_sub = end - start;
        } else {
            len_sub = strlen(start);
        }
        if (len_sub >= MAX_CMD_LEN) len_sub = MAX_CMD_LEN - 1;
        strncpy(substr, start, len_sub);
        substr[len_sub] = '\0';

        free(work);
        const char *cmd = reverse_search_history(substr);
        if (cmd == NULL) {
            fprintf(stderr, "oscdsh: !?%s?: 无匹配历史\n", substr);
            return NULL;
        }
        return strdup(cmd);
    }

    free(work);
    fprintf(stderr, "oscdsh: !: 格式错误\n");
    return NULL;
}

void print_history(void) {
    for (int i = 0; i < history_count; i++) {
        printf("%5d  %s\n", i + 1, osh_history_list[i]);
    }
}