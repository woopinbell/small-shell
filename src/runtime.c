#define _POSIX_C_SOURCE 200809L

#include "runtime.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef SMALL_SHELL_TESTING

static int fail_call(const char *name, unsigned long *calls)
{
    const char      *text;
    char            *end;
    unsigned long   target;

    (*calls)++;
    text = getenv(name);
    if (text == NULL || text[0] == '\0')
        return 0;
    target = strtoul(text, &end, 10);
    return (end != text && *end == '\0' && target != 0 && target == *calls);
}

#endif

int shell_pipe(int fds[2])
{
#ifdef SMALL_SHELL_TESTING
    static unsigned long calls;

    if (fail_call("SMALL_SHELL_FAIL_PIPE", &calls)) {
        errno = EMFILE;
        return -1;
    }
#endif
    return pipe(fds);
}

pid_t shell_fork(void)
{
#ifdef SMALL_SHELL_TESTING
    static unsigned long calls;

    if (fail_call("SMALL_SHELL_FAIL_FORK", &calls)) {
        errno = EAGAIN;
        return -1;
    }
#endif
    return fork();
}

pid_t shell_waitpid(pid_t pid, int *status, int options)
{
#ifdef SMALL_SHELL_TESTING
    static unsigned long calls;

    if (fail_call("SMALL_SHELL_FAIL_WAITPID", &calls)) {
        errno = EIO;
        return -1;
    }
#endif
    return waitpid(pid, status, options);
}
