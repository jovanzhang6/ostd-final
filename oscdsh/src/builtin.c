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