#define _POSIX_C_SOURCE 200809L

#include "shell.h"
#include "runtime.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

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
    errno = 0;
    shell_runtime_set_alloc_scope("startup");
    shell.env = env_from_environ(envp);
    if (shell.env == NULL && envp != NULL && envp[0] != NULL
        && errno == ENOMEM) {
        fprintf(stderr, "small-shell: startup: %s\n", strerror(errno));
        return 1;
    }
    shell.last_status = 0;
    shell.running = 1;

    shell_loop(&shell);
    result = shell.last_status;
    env_free(shell.env);
    return normalize_status(result);
}
