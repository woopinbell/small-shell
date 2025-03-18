#include "shell.h"

#include <stdlib.h>

static void free_redirs(t_redir *redir) {
    t_redir *next;

    while (redir) {
        next = redir->next;
        free(redir->target);
        free(redir);
        redir = next;
    }
}

static void free_commands(t_command *cmd) {
    t_command *next;

    while (cmd) {
        next = cmd->next;
        sh_free_words(cmd->argv);
        free_redirs(cmd->redirs);
        free(cmd);
        cmd = next;
    }
}

void free_pipeline(t_pipeline *pipeline) {
    t_pipeline *next;

    while (pipeline) {
        next = pipeline->next;
        free_commands(pipeline->commands);
        free(pipeline);
        pipeline = next;
    }
}
