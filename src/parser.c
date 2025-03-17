#include "shell.h"

#include <stdlib.h>
#include <string.h>

static void free_commands(t_command *cmd);

static void set_error(char **error, const char *message) {
    if (error)
        *error = sh_strdup(message);
}

static t_command *new_command(void) {
    return (t_command *)sh_xcalloc(1, sizeof(t_command));
}

static t_pipeline *new_pipeline(void) {
    t_pipeline *pipeline = (t_pipeline *)sh_xcalloc(1, sizeof(t_pipeline));
    pipeline->next_op = CONN_NONE;
    return pipeline;
}

static size_t word_count(char **argv) {
    size_t n = 0;
    while (argv && argv[n])
        n++;
    return n;
}

static void add_arg(t_command *cmd, const char *text) {
    size_t  n;
    char    **next;
    size_t  i;

    n = word_count(cmd->argv);
    next = (char **)sh_xcalloc(n + 2, sizeof(char *));
    i = 0;
    while (i < n) {
        next[i] = cmd->argv[i];
        i++;
    }
    next[n] = sh_strdup(text);
    free(cmd->argv);
    cmd->argv = next;
    cmd->argc = n + 1;
}

static void add_redir(t_command *cmd, t_redir_type type, const char *target) {
    t_redir *node;
    t_redir *tail;

    node = (t_redir *)sh_xcalloc(1, sizeof(t_redir));
    node->type = type;
    node->target = sh_strdup(target);
    if (!cmd->redirs) {
        cmd->redirs = node;
        return;
    }
    tail = cmd->redirs;
    while (tail->next)
        tail = tail->next;
    tail->next = node;
}

static int command_empty(t_command *cmd) {
    return (!cmd || (cmd->argc == 0 && !cmd->redirs));
}

static int token_is_redir(t_token_type type) {
    return (type == TOK_REDIR_IN || type == TOK_REDIR_OUT
        || type == TOK_REDIR_APPEND);
}

static t_redir_type redir_type(t_token_type type) {
    if (type == TOK_REDIR_OUT)
        return REDIR_OUT;
    if (type == TOK_REDIR_APPEND)
        return REDIR_APPEND;
    return REDIR_IN;
}

t_pipeline *parse_tokens(t_token *tokens, char **error) {
    t_pipeline  *pipeline;
    t_command   *cmd;
    t_token     *cur;

    pipeline = new_pipeline();
    cmd = new_command();
    cur = tokens;
    if (error)
        *error = NULL;
    while (cur) {
        if (cur->type == TOK_WORD)
            add_arg(cmd, cur->text);
        else if (token_is_redir(cur->type)) {
            if (!cur->next || cur->next->type != TOK_WORD) {
                set_error(error, "syntax error: redirection target missing");
                free_commands(cmd);
                free(pipeline);
                return NULL;
            }
            add_redir(cmd, redir_type(cur->type), cur->next->text);
            cur = cur->next;
        } else {
            set_error(error, "syntax error: unsupported operator");
            free_commands(cmd);
            free(pipeline);
            return NULL;
        }
        cur = cur->next;
    }
    if (command_empty(cmd)) {
        free_commands(cmd);
        free(pipeline);
        return NULL;
    }
    pipeline->commands = cmd;
    pipeline->command_count = 1;
    return pipeline;
}

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
