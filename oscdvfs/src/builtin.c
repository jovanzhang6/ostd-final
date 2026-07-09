#include "oscdvfs.h"

int builtin_hello(char **args) {
    (void)args;
    printf("Hello from VFS!\n");
    return 1;
}

int builtin_exit(char **args) {
    (void)args;
    printf("Bye from VFS!\n");
    exit(0);
}