#ifndef OSCDSH_H
#define OSCDSH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define PROMPT "oscdsh> "

int builtin_hello(char **args);
int builtin_exit(char **args);

int execute_external(char **args);

int execute_command(char *line);

#endif