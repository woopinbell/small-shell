#ifndef EXEC_INTERNAL_H
# define EXEC_INTERNAL_H

# include "shell.h"

struct heredoc_entry {
    const t_redir           *redir;
    char                    *body;
    struct heredoc_entry    *next;
};

struct exec_context {
    t_shell                 *shell;
    struct heredoc_entry    *heredocs;
};

int         exec_prepare_heredocs(struct exec_context *ctx,
                t_pipeline *pipelines);
void        exec_heredoc_entries_free(struct heredoc_entry *entry);
const char  *exec_find_heredoc_body(const struct exec_context *ctx,
                const t_redir *redir);
int         exec_apply_redirections(const t_command *command,
                const struct exec_context *ctx);
int         exec_run_parent_command(t_shell *shell, const t_command *command,
                const struct exec_context *ctx);

#endif
