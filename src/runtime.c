#define _POSIX_C_SOURCE 200809L

#include "runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
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
    if (end == text || *end != '\0' || target == 0)
        return 0;
    {
        char repeat_name[64];
        size_t length;

        length = strlen(name);
        if (length + sizeof("_REPEAT") <= sizeof(repeat_name)) {
            memcpy(repeat_name, name, length);
            memcpy(repeat_name + length, "_REPEAT", sizeof("_REPEAT"));
            if (getenv(repeat_name) != NULL)
                return *calls >= target;
        }
    }
    return target == *calls;
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

int shell_dup(int fd)
{
#ifdef SMALL_SHELL_TESTING
    static unsigned long calls;

    if (fail_call("SMALL_SHELL_FAIL_DUP", &calls)) {
        errno = EMFILE;
        return -1;
    }
#endif
    return dup(fd);
}

int shell_dup2(int oldfd, int newfd)
{
#ifdef SMALL_SHELL_TESTING
    static unsigned long calls;

    if (fail_call("SMALL_SHELL_FAIL_DUP2", &calls)) {
        errno = EIO;
        return -1;
    }
#endif
    return dup2(oldfd, newfd);
}

int shell_open(const char *path, int flags, mode_t mode)
{
#ifdef SMALL_SHELL_TESTING
    static unsigned long calls;

    if (fail_call("SMALL_SHELL_FAIL_OPEN", &calls)) {
        errno = EACCES;
        return -1;
    }
#endif
    return open(path, flags, mode);
}

int shell_fflush(FILE *stream)
{
#ifdef SMALL_SHELL_TESTING
    static unsigned long calls;

    if (fail_call("SMALL_SHELL_FAIL_FFLUSH", &calls)) {
        errno = ENOSPC;
        return EOF;
    }
#endif
    return fflush(stream);
}

int shell_fseek(FILE *stream, long offset, int whence)
{
#ifdef SMALL_SHELL_TESTING
    static unsigned long calls;

    if (fail_call("SMALL_SHELL_FAIL_FSEEK", &calls)) {
        errno = EIO;
        return -1;
    }
#endif
    return fseek(stream, offset, whence);
}

int shell_fileno(FILE *stream)
{
    return fileno(stream);
}
