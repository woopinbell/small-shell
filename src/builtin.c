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
        "export",
        "unset",
        "exit",
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

static int split_assignment(const char *arg, char **key, const char **value)
{
    size_t len;

    len = 0;
    while (arg[len] != '\0' && arg[len] != '=')
        len++;
    *key = shell_strndup(arg, len);
    if (*key == NULL)
        return 1;
    *value = arg[len] == '=' ? arg + len + 1 : NULL;
    return 0;
}

static int builtin_export(t_shell *shell, char **argv)
{
    size_t i;
    int status;

    if (argv[1] == NULL) {
        env_print(shell->env, 1);
        return ferror(stdout) ? 1 : 0;
    }

    status = 0;
    for (i = 1; argv[i] != NULL; i++) {
        char *key;
        const char *value;

        key = NULL;
        value = NULL;
        if (split_assignment(argv[i], &key, &value) != 0) {
            fprintf(stderr, "small-shell: export: allocation failure\n");
            return 1;
        }
        if (!sh_is_name_start((unsigned char)key[0])) {
            fprintf(stderr, "small-shell: export: `%s': not a valid identifier\n", argv[i]);
            free(key);
            status = 1;
            continue;
        }
        {
            size_t j;

            for (j = 1; key[j] != '\0'; j++) {
                if (!sh_is_name_char((unsigned char)key[j])) {
                    fprintf(stderr, "small-shell: export: `%s': not a valid identifier\n", argv[i]);
                    free(key);
                    key = NULL;
                    status = 1;
                    break;
                }
            }
        }
        if (key == NULL)
            continue;
        if (env_set(&shell->env, key, value, 1) != 0) {
            fprintf(stderr, "small-shell: export: allocation failure\n");
            free(key);
            return 1;
        }
        free(key);
    }
    return status;
}

static int builtin_unset(t_shell *shell, char **argv)
{
    size_t i;

    for (i = 1; argv[i] != NULL; i++)
        (void)env_unset(&shell->env, argv[i]);
    return 0;
}

static int parse_exit_status(const char *s, int *status)
{
    char *end;
    long value;

    errno = 0;
    value = strtol(s, &end, 10);
    if (s == end || *end != '\0' || errno == ERANGE)
        return 0;
    *status = (unsigned char)value;
    return 1;
}

static int builtin_exit(t_shell *shell, char **argv)
{
    int status;

    if (argv[1] == NULL) {
        shell->running = 0;
        return shell->last_status;
    }
    if (!parse_exit_status(argv[1], &status)) {
        fprintf(stderr, "small-shell: exit: %s: numeric argument required\n", argv[1]);
        shell->last_status = 2;
        shell->running = 0;
        return 2;
    }
    if (argv[2] != NULL) {
        fprintf(stderr, "small-shell: exit: too many arguments\n");
        return 1;
    }
    shell->last_status = status;
    shell->running = 0;
    return status;
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
    if (strcmp(argv[0], "export") == 0)
        return builtin_export(shell, argv);
    if (strcmp(argv[0], "unset") == 0)
        return builtin_unset(shell, argv);
    if (strcmp(argv[0], "exit") == 0)
        return builtin_exit(shell, argv);
    return 127;
}
