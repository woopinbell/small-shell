#define _POSIX_C_SOURCE 200809L

#include "shell.h"

static int normalize_status(int status)
{
    return status & 0xff;
}

int main(int argc, char **argv, char **envp)
{
    t_shell shell;
    int result;

    (void)argc;
    (void)argv;
    shell.env = env_from_environ(envp);
    shell.last_status = 0;
    shell.running = 1;

    shell_loop(&shell);
    result = shell.last_status;
    env_free(shell.env);
    return normalize_status(result);
}
