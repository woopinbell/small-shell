#define _POSIX_C_SOURCE 200809L

#include "exec_internal.h"
#include "runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int exec_apply_redirections(const t_command *command,
    const struct exec_context *ctx)
{
    const t_redir *redir;

    redir = command->redirs;
    while (redir != NULL) {
        int fd;

        if (redir->type == REDIR_IN) {
            fd = shell_open(redir->target, O_RDONLY, 0);
            if (fd < 0) {
                fprintf(stderr, "small-shell: %s: %s\n", redir->target,
                    strerror(errno));
                return 1;
            }
            if (shell_dup2(fd, STDIN_FILENO) < 0) {
                fprintf(stderr, "small-shell: dup2: %s\n", strerror(errno));
                close(fd);
                return 1;
            }
            close(fd);
        } else if (redir->type == REDIR_OUT
            || redir->type == REDIR_APPEND) {
            int flags;

            flags = O_WRONLY | O_CREAT;
            if (redir->type == REDIR_OUT)
                flags |= O_TRUNC;
            else
                flags |= O_APPEND;
            fd = shell_open(redir->target, flags, 0644);
            if (fd < 0) {
                fprintf(stderr, "small-shell: %s: %s\n", redir->target,
                    strerror(errno));
                return 1;
            }
            if (shell_dup2(fd, STDOUT_FILENO) < 0) {
                fprintf(stderr, "small-shell: dup2: %s\n", strerror(errno));
                close(fd);
                return 1;
            }
            close(fd);
        } else if (redir->type == REDIR_HEREDOC) {
            FILE        *tmp;
            const char  *body;

            tmp = tmpfile();
            if (tmp == NULL) {
                fprintf(stderr, "small-shell: heredoc: %s\n",
                    strerror(errno));
                return 1;
            }
            body = exec_find_heredoc_body(ctx, redir);
            if (body != NULL && fputs(body, tmp) == EOF) {
                fprintf(stderr, "small-shell: heredoc: %s\n",
                    strerror(errno));
                fclose(tmp);
                return 1;
            }
            fflush(tmp);
            rewind(tmp);
            if (shell_dup2(fileno(tmp), STDIN_FILENO) < 0) {
                fprintf(stderr, "small-shell: dup2: %s\n", strerror(errno));
                fclose(tmp);
                return 1;
            }
            fclose(tmp);
        }
        redir = redir->next;
    }
    return 0;
}

static int save_stdio(int saved[2])
{
    saved[0] = shell_dup(STDIN_FILENO);
    saved[1] = shell_dup(STDOUT_FILENO);
    if (saved[0] < 0 || saved[1] < 0) {
        if (saved[0] >= 0)
            close(saved[0]);
        if (saved[1] >= 0)
            close(saved[1]);
        fprintf(stderr, "small-shell: dup: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

static int restore_one(int saved, int target)
{
    int attempts;
    int had_error;

    attempts = 0;
    had_error = 0;
    while (attempts < 2) {
        if (shell_dup2(saved, target) >= 0)
            return had_error;
        if (errno == EINTR)
            continue;
        fprintf(stderr, "small-shell: dup2: %s\n", strerror(errno));
        had_error = 1;
        attempts++;
    }
    return -1;
}

static int restore_stdio(int saved[2])
{
    int input_result;
    int output_result;

    input_result = restore_one(saved[0], STDIN_FILENO);
    output_result = restore_one(saved[1], STDOUT_FILENO);
    close(saved[0]);
    close(saved[1]);
    if (input_result < 0 || output_result < 0)
        return -1;
    return (input_result != 0 || output_result != 0);
}

int exec_run_parent_command(t_shell *shell, const t_command *command,
    const struct exec_context *ctx)
{
    int saved[2];
    int status;

    if (save_stdio(saved) != 0)
        return 1;
    if (exec_apply_redirections(command, ctx) != 0) {
        if (restore_stdio(saved) < 0)
            shell->running = 0;
        return 1;
    }
    if (command->argc == 0)
        status = 0;
    else
        status = builtin_run(shell, command->argv);
    if (fflush(stdout) == EOF)
        status = 1;
    {
        int restore_result;

        restore_result = restore_stdio(saved);
        if (restore_result != 0)
            status = 1;
        if (restore_result < 0)
            shell->running = 0;
    }
    return status;
}
