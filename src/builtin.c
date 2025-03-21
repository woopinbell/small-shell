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
        "cd",
        "env",
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

static size_t argv_count(char **argv)
{
    size_t count;

    count = 0;
    while (argv != NULL && argv[count] != NULL)
        count++;
    return count;
}

static int builtin_cd(t_shell *shell, char **argv)
{
    const char *target;
    char *old_pwd;
    char *new_pwd;
    int print_target;

    if (argv_count(argv) > 2) {
        fprintf(stderr, "small-shell: cd: too many arguments\n");
        return 1;
    }
    print_target = 0;
    if (argv[1] == NULL) {
        target = env_get(shell->env, "HOME");
        if (target == NULL || target[0] == '\0') {
            fprintf(stderr, "small-shell: cd: HOME not set\n");
            return 1;
        }
    } else if (strcmp(argv[1], "-") == 0) {
        target = env_get(shell->env, "OLDPWD");
        if (target == NULL || target[0] == '\0') {
            fprintf(stderr, "small-shell: cd: OLDPWD not set\n");
            return 1;
        }
        print_target = 1;
    } else {
        target = argv[1];
    }
    old_pwd = getcwd(NULL, 0);
    if (chdir(target) != 0) {
        fprintf(stderr, "small-shell: cd: %s: %s\n", target, strerror(errno));
        free(old_pwd);
        return 1;
    }
    new_pwd = getcwd(NULL, 0);
    if (old_pwd != NULL)
        (void)env_set(&shell->env, "OLDPWD", old_pwd, 1);
    if (new_pwd != NULL)
        (void)env_set(&shell->env, "PWD", new_pwd, 1);
    if (print_target && new_pwd != NULL)
        printf("%s\n", new_pwd);
    free(old_pwd);
    free(new_pwd);
    return ferror(stdout) ? 1 : 0;
}

static int builtin_env(t_shell *shell, char **argv)
{
    if (argv[1] != NULL) {
        fprintf(stderr, "small-shell: env: arguments are not supported\n");
        return 1;
    }
    env_print(shell->env, 0);
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
    if (strcmp(argv[0], "cd") == 0)
        return builtin_cd(shell, argv);
    if (strcmp(argv[0], "env") == 0)
        return builtin_env(shell, argv);
    return 127;
}
