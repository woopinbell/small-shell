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

    (void)ctx;
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
        }
        redir = redir->next;
    }
    return 0;
}
