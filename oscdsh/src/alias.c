// oscdsh/src/alias.c
#include "oscdsh.h"

typedef struct {
    char name[128];
    char value[1024];
} alias_t;

static alias_t aliases[MAX_ALIASES];
static int alias_count = 0;

void add_alias(const char *name, const char *value) {
    // 检查是否已存在，存在则更新
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            strncpy(aliases[i].value, value, sizeof(aliases[i].value) - 1);
            aliases[i].value[sizeof(aliases[i].value) - 1] = '\0';
            return;
        }
    }
    // 新增
    if (alias_count >= MAX_ALIASES) {
        fprintf(stderr, "oscdsh: 别名数量已达上限\n");
        return;
    }
    strncpy(aliases[alias_count].name, name, sizeof(aliases[alias_count].name) - 1);
    aliases[alias_count].name[sizeof(aliases[alias_count].name) - 1] = '\0';
    strncpy(aliases[alias_count].value, value, sizeof(aliases[alias_count].value) - 1);
    aliases[alias_count].value[sizeof(aliases[alias_count].value) - 1] = '\0';
    alias_count++;
}

int remove_alias(const char *name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            // 将最后一个移到当前位置（或直接前移）
            aliases[i] = aliases[alias_count - 1];
            alias_count--;
            return 0;
        }
    }
    return -1;  // 未找到
}

const char *get_alias_value(const char *name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            return aliases[i].value;
        }
    }
    return NULL;
}

void print_aliases(void) {
    for (int i = 0; i < alias_count; i++) {
        printf("%s='%s'\n", aliases[i].name, aliases[i].value);
    }
}

void print_one_alias(const char *name) {
    const char *val = get_alias_value(name);
    if (val) {
        printf("%s='%s'\n", name, val);
    } else {
        printf("alias: %s: 未找到\n", name);
    }
}