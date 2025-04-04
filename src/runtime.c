#define _POSIX_C_SOURCE 200809L

#include "runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef SMALL_SHELL_TESTING

static const char       *g_alloc_scope;
static unsigned long    g_alloc_calls;
static int              g_alloc_failed;
static unsigned long    g_command_number;

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

static int fail_allocation(void)
{
    const char      *command_text;
    const char      *scope;
    const char      *text;
    char            *end;
    unsigned long   target;
    unsigned long   target_command;
    int             repeat;

    if (g_alloc_failed)
        return 0;
    command_text = getenv("SMALL_SHELL_FAIL_ALLOC_COMMAND");
    target_command = command_text != NULL ? strtoul(command_text, NULL, 10) : 1;
    if (target_command == 0 || g_command_number != target_command)
        return 0;
    scope = getenv("SMALL_SHELL_FAIL_ALLOC_SCOPE");
    if (scope == NULL || g_alloc_scope == NULL
        || strcmp(scope, g_alloc_scope) != 0)
        return 0;
    g_alloc_calls++;
    text = getenv("SMALL_SHELL_FAIL_ALLOC");
    target = 1;
    if (text != NULL && text[0] != '\0') {
        target = strtoul(text, &end, 10);
        if (end == text || *end != '\0' || target == 0)
            return 0;
    }
    repeat = getenv("SMALL_SHELL_FAIL_ALLOC_REPEAT") != NULL;
    if ((!repeat && g_alloc_calls != target)
        || (repeat && g_alloc_calls < target))
        return 0;
    if (!repeat)
        g_alloc_failed = 1;
    errno = ENOMEM;
    return 1;
}

#endif

void shell_runtime_begin_command(void)
{
#ifdef SMALL_SHELL_TESTING
    g_command_number++;
    g_alloc_calls = 0;
#endif
}

void shell_runtime_set_alloc_scope(const char *scope)
{
#ifdef SMALL_SHELL_TESTING
    g_alloc_scope = scope;
#else
    (void)scope;
#endif
}

void *shell_malloc(size_t size)
{
#ifdef SMALL_SHELL_TESTING
    if (fail_allocation())
        return NULL;
#endif
    return malloc(size);
}

void *shell_calloc(size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size) {
        errno = ENOMEM;
        return NULL;
    }
#ifdef SMALL_SHELL_TESTING
    if (fail_allocation())
        return NULL;
#endif
    return calloc(count, size);
}

void *shell_realloc(void *ptr, size_t size)
{
#ifdef SMALL_SHELL_TESTING
    if (fail_allocation())
        return NULL;
#endif
    return realloc(ptr, size);
}

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

ssize_t shell_read(int fd, void *buffer, size_t size)
{
    return read(fd, buffer, size);
}

ssize_t shell_write(int fd, const void *buffer, size_t size)
{
    return write(fd, buffer, size);
}

int shell_write_all(int fd, const void *buffer, size_t size)
{
    const unsigned char *cursor;

    cursor = (const unsigned char *)buffer;
    while (size > 0) {
        ssize_t written;

        written = shell_write(fd, cursor, size);
        if (written > 0) {
            cursor += (size_t)written;
            size -= (size_t)written;
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            if (written == 0)
                errno = EIO;
            return 1;
        }
    }
    return 0;
}

int shell_write_text(int fd, const char *text)
{
    if (text == NULL)
        return 0;
    return shell_write_all(fd, text, strlen(text));
}
