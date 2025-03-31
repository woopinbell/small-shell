#define _POSIX_C_SOURCE 200809L

#include "exec_internal.h"
#include "runtime.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static void close_pipes(int (*pipes)[2], size_t pipe_count)
{
    size_t i;

    if (pipes == NULL)
        return;
    for (i = 0; i < pipe_count; i++) {
        if (pipes[i][0] >= 0)
            close(pipes[i][0]);
        if (pipes[i][1] >= 0)
            close(pipes[i][1]);
    }
}

static int status_from_wait(int wait_status)
{
    if (WIFEXITED(wait_status))
        return WEXITSTATUS(wait_status);
    if (WIFSIGNALED(wait_status))
        return 128 + WTERMSIG(wait_status);
    return 1;
}

static void child_die(const char *what)
{
    fprintf(stderr, "small-shell: %s: %s\n", what, strerror(errno));
    _exit(1);
}

static void run_child(t_shell *shell, const t_pipeline *pipeline, const t_command *command,
    const struct exec_context *ctx, int (*pipes)[2], size_t pipe_count, size_t index)
{
    if (index > 0 && shell_dup2(pipes[index - 1][0], STDIN_FILENO) < 0)
        child_die("dup2");
    if (index + 1 < pipeline->command_count && shell_dup2(pipes[index][1], STDOUT_FILENO) < 0)
        child_die("dup2");
    close_pipes(pipes, pipe_count);

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
            fprintf(stderr, "small-shell: %s: %s\n", command->argv[0], strerror(err));
            sh_free_words(envp);
            if (err == ENOENT)
                _exit(127);
            _exit(126);
        }
    }
}

static void terminate_children(const pid_t *pids, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (pids[i] > 0 && kill(pids[i], SIGKILL) < 0 && errno != ESRCH)
            fprintf(stderr, "small-shell: kill: %s\n", strerror(errno));
    }
}

static int wait_for_child(pid_t pid, int *wait_status)
{
    int attempts;
    int had_error;

    attempts = 0;
    had_error = 0;
    while (attempts < 2) {
        pid_t waited;

        waited = shell_waitpid(pid, wait_status, 0);
        if (waited == pid)
            return had_error;
        if (waited < 0 && errno == EINTR)
            continue;
        fprintf(stderr, "small-shell: waitpid: %s\n", strerror(errno));
        had_error = 1;
        attempts++;
    }
    return -1;
}

static int run_forked_pipeline(t_shell *shell, const t_pipeline *pipeline, const struct exec_context *ctx)
{
    size_t pipe_count;
    int (*pipes)[2];
    pid_t *pids;
    const t_command *command;
    size_t i;
    size_t spawned;
    int result;
    int wait_error;

    pipe_count = pipeline->command_count - 1;
    pipes = NULL;
    pids = NULL;
    spawned = 0;
    result = 1;
    wait_error = 0;

    if (pipe_count > 0) {
        pipes = (int (*)[2])shell_malloc(sizeof(int[2]) * pipe_count);
        if (pipes == NULL)
            goto alloc_error;
        for (i = 0; i < pipe_count; i++) {
            pipes[i][0] = -1;
            pipes[i][1] = -1;
        }
        for (i = 0; i < pipe_count; i++) {
            if (shell_pipe(pipes[i]) < 0) {
                fprintf(stderr, "small-shell: pipe: %s\n", strerror(errno));
                close_pipes(pipes, pipe_count);
                free(pipes);
                return 1;
            }
        }
    }

    pids = (pid_t *)shell_calloc(pipeline->command_count, sizeof(pid_t));
    if (pids == NULL)
        goto alloc_error;

    command = pipeline->commands;
    for (i = 0; i < pipeline->command_count && command != NULL; i++) {
        pid_t pid;

        pid = shell_fork();
        if (pid < 0) {
            fprintf(stderr, "small-shell: fork: %s\n", strerror(errno));
            break;
        }
        if (pid == 0)
            run_child(shell, pipeline, command, ctx, pipes, pipe_count, i);
        pids[i] = pid;
        spawned++;
        command = command->next;
    }

    close_pipes(pipes, pipe_count);
    if (spawned != pipeline->command_count)
        terminate_children(pids, spawned);
    for (i = 0; i < spawned; i++) {
        int wait_status;
        int wait_result;

        wait_result = wait_for_child(pids[i], &wait_status);
        if (wait_result == 0 && i + 1 == pipeline->command_count)
            result = status_from_wait(wait_status);
        if (wait_result != 0)
            wait_error = 1;
    }

    if (wait_error)
        result = 1;
    free(pids);
    free(pipes);
    return spawned == pipeline->command_count ? result : 1;

alloc_error:
    fprintf(stderr, "small-shell: allocation failure\n");
    close_pipes(pipes, pipe_count);
    free(pipes);
    free(pids);
    return 1;
}

static int expand_one_pipeline(t_shell *shell, t_pipeline *pipeline)
{
    t_pipeline *next;
    int result;

    next = pipeline->next;
    pipeline->next = NULL;
    result = expand_pipeline(shell, pipeline);
    pipeline->next = next;
    return result;
}

static int execute_one_pipeline(t_shell *shell, t_pipeline *pipeline, const struct exec_context *ctx)
{
    const t_command *command;

    if (pipeline == NULL || pipeline->command_count == 0)
        return shell->last_status;
    if (expand_one_pipeline(shell, pipeline) != 0)
        return 1;
    command = pipeline->commands;
    if (pipeline->command_count == 1
        && (command->argc == 0 || builtin_is_parent(command->argv[0])))
        return exec_run_parent_command(shell, command, ctx);
    return run_forked_pipeline(shell, pipeline, ctx);
}

static int execute_pipeline_list_ctx(t_shell *shell, t_pipeline *pipeline, const struct exec_context *ctx)
{
    t_connector previous;

    previous = CONN_NONE;
    while (pipeline != NULL && shell->running) {
        int should_run;

        should_run = 1;
        if (previous == CONN_AND && shell->last_status != 0)
            should_run = 0;
        else if (previous == CONN_OR && shell->last_status == 0)
            should_run = 0;
        if (should_run)
            shell->last_status = execute_one_pipeline(shell, pipeline, ctx);
        previous = pipeline->next_op;
        pipeline = pipeline->next;
    }
    return shell->last_status;
}

int execute_pipeline_list(t_shell *shell, t_pipeline *pipeline)
{
    struct exec_context ctx;

    ctx.shell = shell;
    ctx.heredocs = NULL;
    return execute_pipeline_list_ctx(shell, pipeline, &ctx);
}

int shell_process_line(t_shell *shell, const char *line)
{
    t_token *tokens;
    t_pipeline *pipelines;
    struct exec_context ctx;
    char *error;

    if (shell == NULL || line == NULL || line[0] == '\0')
        return shell != NULL ? shell->last_status : 1;

    error = NULL;
    tokens = tokenize_line(line, &error);
    if (error != NULL) {
        fprintf(stderr, "small-shell: %s\n", error);
        free(error);
        shell->last_status = 258;
        return shell->last_status;
    }

    pipelines = parse_tokens(tokens, &error);
    free_tokens(tokens);
    if (error != NULL) {
        fprintf(stderr, "small-shell: %s\n", error);
        free(error);
        shell->last_status = 258;
        return shell->last_status;
    }
    if (pipelines == NULL)
        return shell->last_status;

    ctx.shell = shell;
    ctx.heredocs = NULL;
    if (exec_prepare_heredocs(&ctx, pipelines) != 0) {
        exec_heredoc_entries_free(ctx.heredocs);
        free_pipeline(pipelines);
        shell->last_status = 1;
        return shell->last_status;
    }

    (void)execute_pipeline_list_ctx(shell, pipelines, &ctx);
    exec_heredoc_entries_free(ctx.heredocs);
    free_pipeline(pipelines);
    return shell->last_status;
}
