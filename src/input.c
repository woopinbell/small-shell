#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef USE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

static char *read_plain_line(const char *prompt, int interactive)
{
    size_t cap;
    size_t len;
    char *line;
    int ch;

    if (interactive && prompt != NULL) {
        fputs(prompt, stderr);
        fflush(stderr);
    }

    cap = 128;
    len = 0;
    line = malloc(cap);
    if (line == NULL)
        return NULL;

    while ((ch = fgetc(stdin)) != EOF) {
        char *grown;

        if (ch == '\n')
            break;
        if (len + 1 >= cap) {
            cap *= 2;
            grown = realloc(line, cap);
            if (grown == NULL) {
                free(line);
                return NULL;
            }
            line = grown;
        }
        line[len++] = (char)ch;
    }

    if (ch == EOF && len == 0) {
        free(line);
        return NULL;
    }

    line[len] = '\0';
    return line;
}

char *shell_read_line(const char *prompt, int interactive)
{
#ifdef USE_READLINE
    if (interactive) {
        char *line;

        line = readline(prompt != NULL ? prompt : "");
        if (line != NULL && line[0] != '\0')
            add_history(line);
        return line;
    }
#endif
    return read_plain_line(prompt, interactive);
}

void shell_loop(t_shell *shell)
{
    int interactive;
    char *line;

    if (shell == NULL)
        return;
    interactive = isatty(STDIN_FILENO) && isatty(STDERR_FILENO);
    while (shell->running) {
        line = shell_read_line("small-shell$ ", interactive);
        if (line == NULL)
            break;
        (void)shell_process_line(shell, line);
        free(line);
    }
    if (interactive && shell->running)
        fputc('\n', stderr);
}
