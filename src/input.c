#define _POSIX_C_SOURCE 200809L

#include "shell.h"
#include "runtime.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

static char *read_plain_line(const char *prompt, int interactive, int *failed)
{
    size_t  cap;
    size_t  len;
    char    *line;

    *failed = 0;
    if (interactive && prompt != NULL) {
        fputs(prompt, stderr);
        fflush(stderr);
    }
    cap = 128;
    len = 0;
    line = (char *)shell_malloc(cap);
    if (line == NULL) {
        *failed = 1;
        return NULL;
    }
    for (;;) {
        unsigned char   ch;
        ssize_t         count;

        count = shell_read(STDIN_FILENO, &ch, 1);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0) {
            free(line);
            *failed = 1;
            return NULL;
        }
        if (count == 0) {
            if (len == 0) {
                free(line);
                return NULL;
            }
            break;
        }
        if (ch == '\n')
            break;
        if (len + 1 >= cap) {
            char *grown;

            if (cap > SIZE_MAX / 2) {
                free(line);
                errno = ENOMEM;
                *failed = 1;
                return NULL;
            }
            cap *= 2;
            grown = (char *)shell_realloc(line, cap);
            if (grown == NULL) {
                free(line);
                *failed = 1;
                return NULL;
            }
            line = grown;
        }
        line[len++] = (char)ch;
    }
    line[len] = '\0';
    return line;
}

char *shell_read_line(const char *prompt, int interactive, int *failed)
{
#ifdef USE_READLINE
    if (interactive) {
        char *line;

        *failed = 0;
        line = readline(prompt != NULL ? prompt : "");
        if (line != NULL && line[0] != '\0')
            add_history(line);
        return line;
    }
#endif
    return read_plain_line(prompt, interactive, failed);
}

void shell_loop(t_shell *shell)
{
    int     interactive;
    char    *line;

    if (shell == NULL)
        return;
    interactive = isatty(STDIN_FILENO) && isatty(STDERR_FILENO);
    while (shell->running) {
        int failed;

        line = shell_read_line("small-shell$ ", interactive, &failed);
        if (line == NULL) {
            if (failed) {
                fprintf(stderr, "small-shell: input: %s\n", strerror(errno));
                shell->last_status = 1;
            }
            break;
        }
        (void)shell_process_line(shell, line);
        free(line);
    }
    if (interactive && shell->running)
        fputc('\n', stderr);
}
