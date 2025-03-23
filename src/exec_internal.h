#ifndef EXEC_INTERNAL_H
# define EXEC_INTERNAL_H

# include "shell.h"

struct exec_context {
    t_shell *shell;
};

int exec_apply_redirections(const t_command *command,
        const struct exec_context *ctx);
int exec_run_parent_command(t_shell *shell, const t_command *command,
        const struct exec_context *ctx);

#endif
