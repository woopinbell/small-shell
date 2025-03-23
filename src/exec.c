#define _POSIX_C_SOURCE 200809L

#include "exec_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int status_from_wait(int wait_status)
{
    if (WIFEXITED(wait_status))
        return WEXITSTATUS(wait_status);
    if (WIFSIGNALED(wait_status))
        return 128 + WTERMSIG(wait_status);
    return 1;
}

static void run_child(t_shell *shell, const t_command *command,
    const struct exec_context *ctx)
{
    if (exec_apply_redirections(command, ctx) != 0)
        _exit(1);
    if (command->argc == 0)
        _exit(0);
    if (builtin_is_known(command->argv[0])) {
        int status;

        status = builtin_run(shell, command->argv);
        fflush(stdout);
        fflush(stderr);
        _exit(status & 0xff);
    }
    {
        char **envp;

        envp = env_to_environ(shell->env);
        if (envp == NULL) {
            fprintf(stderr, "small-shell: allocation failure\n");
            _exit(1);
        }
        environ = envp;
        execvp(command->argv[0], command->argv);
        {
            int err;

            err = errno;
            fprintf(stderr, "small-shell: %s: %s\n", command->argv[0],
                strerror(err));
            sh_free_words(envp);
            if (err == ENOENT)
                _exit(127);
            _exit(126);
        }
    }
}

static int run_single_command(t_shell *shell, const t_command *command,
    const struct exec_context *ctx)
{
    pid_t pid;
    pid_t waited;
    int wait_status;

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "small-shell: fork: %s\n", strerror(errno));
        return 1;
    }
    if (pid == 0)
        run_child(shell, command, ctx);
    do {
        waited = waitpid(pid, &wait_status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited != pid)
        return 1;
    return status_from_wait(wait_status);
}

int execute_pipeline_list(t_shell *shell, t_pipeline *pipeline)
{
    const t_command *command;
    struct exec_context ctx;

    if (shell == NULL || pipeline == NULL || pipeline->command_count == 0)
        return shell != NULL ? shell->last_status : 1;
    if (pipeline->next != NULL || pipeline->command_count != 1)
        return 1;
    if (expand_pipeline(shell, pipeline) != 0)
        return 1;
    ctx.shell = shell;
    command = pipeline->commands;
    if (command->argc == 0 || builtin_is_parent(command->argv[0]))
        shell->last_status = exec_run_parent_command(shell, command, &ctx);
    else
        shell->last_status = run_single_command(shell, command, &ctx);
    return shell->last_status;
}
