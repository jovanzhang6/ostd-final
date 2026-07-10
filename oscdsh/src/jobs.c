// oscdsh/src/jobs.c
#include "oscdsh.h"

typedef struct {
    int job_id;
    pid_t pid;
    char command[1024];
    int running;
    int notified;
} job_t;

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
    if (job_count >= MAX_JOBS) {
        int new_count = 0;
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].notified) continue;
            if (i != new_count) jobs[new_count] = jobs[i];
            new_count++;
        }
        if (new_count == MAX_JOBS) {
            fprintf(stderr, "oscdsh: 后台作业数已达上限\n");
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
                    char msg[512];
                    // 使用 snprintf 构造输出字符串
                    int len = snprintf(msg, sizeof(msg), "\n[%d]  Done    %s\n",
                                       jobs[i].job_id, jobs[i].command);
                    // 防止截断导致输出不完整
                    if (len >= (int)sizeof(msg))
                        len = sizeof(msg) - 1;
                    write(STDOUT_FILENO, msg, len);
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
        const char *status = jobs[i].running ? "Running" : "Done";
        printf("[%d]  %s    %s\n", jobs[i].job_id, status, jobs[i].command);
    }
}