#define _POSIX_C_SOURCE 200809L

#include "exec_internal.h"

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
            fd = open(redir->target, O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "small-shell: %s: %s\n", redir->target,
                    strerror(errno));
                return 1;
            }
            if (dup2(fd, STDIN_FILENO) < 0) {
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
            fd = open(redir->target, flags, 0644);
            if (fd < 0) {
                fprintf(stderr, "small-shell: %s: %s\n", redir->target,
                    strerror(errno));
                return 1;
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {
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
            if (dup2(fileno(tmp), STDIN_FILENO) < 0) {
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
    saved[0] = dup(STDIN_FILENO);
    saved[1] = dup(STDOUT_FILENO);
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

static void restore_stdio(int saved[2])
{
    if (saved[0] >= 0) {
        (void)dup2(saved[0], STDIN_FILENO);
        close(saved[0]);
    }
    if (saved[1] >= 0) {
        (void)dup2(saved[1], STDOUT_FILENO);
        close(saved[1]);
    }
}

int exec_run_parent_command(t_shell *shell, const t_command *command,
    const struct exec_context *ctx)
{
    int saved[2];
    int status;

    if (save_stdio(saved) != 0)
        return 1;
    if (exec_apply_redirections(command, ctx) != 0) {
        restore_stdio(saved);
        return 1;
    }
    if (command->argc == 0)
        status = 0;
    else
        status = builtin_run(shell, command->argv);
    fflush(stdout);
    restore_stdio(saved);
    return status;
}
