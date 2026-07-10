// oscdfs/src/builtin.c
#include "oscdfs.h"

int builtin_hello(char **args) {
    (void)args;
    printf("Hello from oscdfs!\n");
    return 0;  /* 成功返回0 */
}

int builtin_exit(char **args) {
    (void)args;
    printf("Bye from oscdfs!\n");
    exit(0);
    /* 不会执行到这里，但返回值可设为0保持统一 */
    return 0;
}