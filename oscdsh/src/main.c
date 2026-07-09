// src/main.c
#include "oscdsh.h"
#include "jobs.h"
#include <signal.h>

int main() {
    char line[MAX_CMD_LEN];

    init_jobs();

    signal(SIGCHLD, sigchld_handler);

    printf("Operating System Course Design Shell\n\n");

    while (1) {
        printf(PROMPT);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0') continue;

        execute_command(line);
    }

    return 0;
}