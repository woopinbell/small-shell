#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int builtin_is_known(const char *name)
{
    static const char *builtins[] = {
        "echo",
        "pwd",
        NULL
    };
    size_t i;

    if (name == NULL)
        return 0;
    for (i = 0; builtins[i] != NULL; i++) {
        if (strcmp(name, builtins[i]) == 0)
            return 1;
    }
    return 0;
}

int builtin_is_parent(const char *name)
{
    return builtin_is_known(name);
}

static int builtin_echo(char **argv)
{
    size_t i;
    int newline;

    newline = 1;
    i = 1;
    while (argv[i] != NULL && argv[i][0] == '-' && argv[i][1] == 'n') {
        size_t j;
        int only_n;

        only_n = 1;
        for (j = 1; argv[i][j] != '\0'; j++) {
            if (argv[i][j] != 'n') {
                only_n = 0;
                break;
            }
        }
        if (!only_n)
            break;
        newline = 0;
        i++;
    }
    while (argv[i] != NULL) {
        fputs(argv[i], stdout);
        if (argv[i + 1] != NULL)
            fputc(' ', stdout);
        i++;
    }
    if (newline)
        fputc('\n', stdout);
    return ferror(stdout) ? 1 : 0;
}

static int builtin_pwd(void)
{
    char *cwd;

    cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        fprintf(stderr, "small-shell: pwd: %s\n", strerror(errno));
        return 1;
    }
    printf("%s\n", cwd);
    free(cwd);
    return ferror(stdout) ? 1 : 0;
}

int builtin_run(t_shell *shell, char **argv)
{
    if (shell == NULL || argv == NULL || argv[0] == NULL)
        return 0;
    if (strcmp(argv[0], "echo") == 0)
        return builtin_echo(argv);
    if (strcmp(argv[0], "pwd") == 0)
        return builtin_pwd();
    return 127;
}
