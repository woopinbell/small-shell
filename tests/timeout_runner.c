#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t pending_signal;

static void record_signal(int signal_number)
{
    if (pending_signal == 0)
        pending_signal = signal_number;
}

static int prepare_signals(sigset_t *blocked, sigset_t *previous)
{
    struct sigaction action;

    sigemptyset(blocked);
    sigaddset(blocked, SIGHUP);
    sigaddset(blocked, SIGINT);
    sigaddset(blocked, SIGTERM);
    if (sigprocmask(SIG_BLOCK, blocked, previous) != 0)
        return 1;
    sigemptyset(&action.sa_mask);
    action.sa_handler = record_signal;
    action.sa_flags = 0;
    if (sigaction(SIGHUP, &action, NULL) != 0
        || sigaction(SIGINT, &action, NULL) != 0
        || sigaction(SIGTERM, &action, NULL) != 0) {
        (void)sigprocmask(SIG_SETMASK, previous, NULL);
        return 1;
    }
    return 0;
}

static int prepare_child(const sigset_t *previous)
{
    struct sigaction action;

    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_DFL;
    action.sa_flags = 0;
    if (sigaction(SIGHUP, &action, NULL) != 0
        || sigaction(SIGINT, &action, NULL) != 0
        || sigaction(SIGTERM, &action, NULL) != 0
        || sigprocmask(SIG_SETMASK, previous, NULL) != 0)
        return 1;
    return 0;
}

static long long monotonic_milliseconds(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return -1;
    return (long long)now.tv_sec * 1000LL + now.tv_nsec / 1000000LL;
}

static int child_status(int status)
{
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
}

static int parse_seconds(const char *text, long *seconds)
{
    char *end;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (text == end || *end != '\0' || errno == ERANGE
        || value <= 0 || value > 3600)
        return 1;
    *seconds = value;
    return 0;
}

static void terminate_child_group(pid_t pid)
{
    pid_t waited;

    (void)kill(-pid, SIGKILL);
    (void)kill(pid, SIGKILL);
    do {
        waited = waitpid(pid, NULL, 0);
    } while (waited < 0 && errno == EINTR);
}

int main(int argc, char **argv)
{
    long        seconds;
    long long   deadline;
    pid_t       pid;
    sigset_t    blocked;
    sigset_t    previous;

    if (argc < 3 || parse_seconds(argv[1], &seconds) != 0) {
        fprintf(stderr, "usage: timeout-runner SECONDS COMMAND [ARG...]\n");
        return 2;
    }
    deadline = monotonic_milliseconds();
    if (deadline < 0)
        return 2;
    deadline += (long long)seconds * 1000LL;
    if (prepare_signals(&blocked, &previous) != 0)
        return 2;
    pid = fork();
    if (pid < 0) {
        (void)sigprocmask(SIG_SETMASK, &previous, NULL);
        return 2;
    }
    if (pid == 0) {
        (void)setpgid(0, 0);
        if (prepare_child(&previous) != 0)
            _exit(126);
        execvp(argv[2], &argv[2]);
        _exit(errno == ENOENT ? 127 : 126);
    }
    if (setpgid(pid, pid) != 0 && errno != EACCES && errno != ESRCH) {
        terminate_child_group(pid);
        (void)sigprocmask(SIG_SETMASK, &previous, NULL);
        return 2;
    }
    if (sigprocmask(SIG_SETMASK, &previous, NULL) != 0) {
        terminate_child_group(pid);
        return 2;
    }
    for (;;) {
        int     status;
        int     signal_number;
        long long now;
        pid_t   waited;

        signal_number = pending_signal;
        if (signal_number != 0) {
            terminate_child_group(pid);
            return 128 + signal_number;
        }
        waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid)
            return child_status(status);
        if (waited < 0 && errno != EINTR) {
            terminate_child_group(pid);
            return 2;
        }
        now = monotonic_milliseconds();
        if (now < 0) {
            terminate_child_group(pid);
            return 2;
        }
        if (now >= deadline) {
            terminate_child_group(pid);
            return 124;
        }
        {
            struct timespec pause_time;

            pause_time.tv_sec = 0;
            pause_time.tv_nsec = 10000000L;
            while (nanosleep(&pause_time, &pause_time) != 0
                && errno == EINTR) {
            }
        }
    }
}
