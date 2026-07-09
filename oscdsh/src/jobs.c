// src/jobs.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "jobs.h"

static job_t jobs[MAX_JOBS];
static int job_count = 0;
static int next_job_id = 1;

void init_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        jobs[i].running = 0;
        jobs[i].notified = 0;
    }
    job_count = 0;
}

int add_job(pid_t pid, const char *cmdline) {
    // 若表已满，先清理所有已通知（已完成）的作业，腾出空间
    if (job_count >= MAX_JOBS) {
        int new_count = 0;
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].notified) {
                continue;   // 跳过已通知的作业
            }
            if (i != new_count) {
                jobs[new_count] = jobs[i];
            }
            new_count++;
        }
        if (new_count == MAX_JOBS) {
            fprintf(stderr, "oscdsh: 后台作业数已达上限，无法添加新作业\n");
            return -1;
        }
        job_count = new_count;
    }

    int jid = next_job_id++;
    jobs[job_count].job_id = jid;
    jobs[job_count].pid = pid;
    strncpy(jobs[job_count].command, cmdline, sizeof(jobs[job_count].command) - 1);
    jobs[job_count].command[sizeof(jobs[job_count].command) - 1] = '\0';
    jobs[job_count].running = 1;
    jobs[job_count].notified = 0;
    job_count++;
    return jid;
}

void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].running && jobs[i].pid == pid) {
                jobs[i].running = 0;
                if (!jobs[i].notified) {
                    printf("\n[%d]  Done    %s\n", jobs[i].job_id, jobs[i].command);
                    fflush(stdout);
                    jobs[i].notified = 1;
                }
                break;
            }
        }
    }
    errno = saved_errno;
}

void print_jobs(void) {
    for (int i = 0; i < job_count; i++) {
        // 只有运行中的或已完成但尚未通知的作业才会显示（已完成且已通知的已被清理或被跳过？不，我们保留，但显示为 Done）
        // 为了直观，我们显示所有尚未清理的作业
        const char *status = jobs[i].running ? "Running" : "Done";
        printf("[%d]  %s    %s\n", jobs[i].job_id, status, jobs[i].command);
    }
}