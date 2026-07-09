// include/jobs.h
#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include <signal.h>

#define MAX_JOBS 64

typedef struct {
    int job_id;
    pid_t pid;
    char command[1024];
    int running;
    int notified;
} job_t;

void init_jobs(void);
int  add_job(pid_t pid, const char *cmdline);
void sigchld_handler(int sig);
void print_jobs(void);

#endif