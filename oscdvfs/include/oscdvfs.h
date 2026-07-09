#ifndef OSCDVFS_H
#define OSCDVFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CMD_LEN 1024
#define PROMPT "oscdvfs> "

int builtin_hello(char **args);
int builtin_exit(char **args);

int execute_command(char *line);

#endif