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

static void add_redir(t_command *cmd, t_redir_type type, const char *target,
        int target_quoted) {
    t_redir *node;
    t_redir *tail;

    node = (t_redir *)sh_xcalloc(1, sizeof(t_redir));
    node->type = type;
    node->target = sh_strdup(target);
    node->heredoc_quoted = (type == REDIR_HEREDOC && target_quoted);
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

static void append_command(t_pipeline *pipeline, t_command *cmd) {
    t_command *tail;

    if (!pipeline->commands) {
        pipeline->commands = cmd;
        pipeline->command_count++;
        return;
    }
    tail = pipeline->commands;
    while (tail->next)
        tail = tail->next;
    tail->next = cmd;
    pipeline->command_count++;
}

static void append_pipeline(t_pipeline **head, t_pipeline **tail, t_pipeline *node) {
    if (!*head)
        *head = node;
    else
        (*tail)->next = node;
    *tail = node;
}

static int token_is_redir(t_token_type type) {
    return (type == TOK_REDIR_IN || type == TOK_REDIR_OUT
        || type == TOK_REDIR_APPEND || type == TOK_HEREDOC);
}

static t_redir_type redir_type(t_token_type type) {
    if (type == TOK_REDIR_OUT)
        return REDIR_OUT;
    if (type == TOK_REDIR_APPEND)
        return REDIR_APPEND;
    if (type == TOK_HEREDOC)
        return REDIR_HEREDOC;
    return REDIR_IN;
}

static t_connector connector_type(t_token_type type) {
    if (type == TOK_AND)
        return CONN_AND;
    if (type == TOK_OR)
        return CONN_OR;
    return CONN_SEQ;
}

t_pipeline *parse_tokens(t_token *tokens, char **error) {
    t_pipeline  *head;
    t_pipeline  *tail;
    t_pipeline  *pipeline;
    t_command   *cmd;
    t_token     *cur;
    t_token_type last_connector;
    int         after_pipe;

    head = NULL;
    tail = NULL;
    pipeline = new_pipeline();
    cmd = new_command();
    cur = tokens;
    last_connector = TOK_WORD;
    after_pipe = 0;
    if (error)
        *error = NULL;
    while (cur) {
        if (cur->type == TOK_WORD) {
            add_arg(cmd, cur->text);
            after_pipe = 0;
        } else if (token_is_redir(cur->type)) {
            if (!cur->next || cur->next->type != TOK_WORD) {
                set_error(error, "syntax error: redirection target missing");
                free_commands(cmd);
                free_pipeline(pipeline);
                free_pipeline(head);
                return NULL;
            }
            add_redir(cmd, redir_type(cur->type), cur->next->text,
                cur->next->quoted);
            cur = cur->next;
            after_pipe = 0;
        } else if (cur->type == TOK_PIPE) {
            if (command_empty(cmd)) {
                set_error(error, "syntax error: empty command before pipe");
                free_commands(cmd);
                free_pipeline(pipeline);
                free_pipeline(head);
                return NULL;
            }
            append_command(pipeline, cmd);
            cmd = new_command();
            after_pipe = 1;
        } else {
            if (after_pipe) {
                set_error(error, "syntax error: expected command after pipe");
                free_commands(cmd);
                free_pipeline(pipeline);
                free_pipeline(head);
                return NULL;
            }
            if (command_empty(cmd) && !pipeline->commands) {
                set_error(error, "syntax error: empty command before connector");
                free_commands(cmd);
                free_pipeline(pipeline);
                free_pipeline(head);
                return NULL;
            }
            if (!command_empty(cmd))
                append_command(pipeline, cmd);
            else
                free_commands(cmd);
            cmd = new_command();
            pipeline->next_op = connector_type(cur->type);
            append_pipeline(&head, &tail, pipeline);
            last_connector = cur->type;
            pipeline = new_pipeline();
            after_pipe = 0;
        }
        cur = cur->next;
    }
    if (after_pipe) {
        set_error(error, "syntax error: expected command after pipe");
        free_commands(cmd);
        free_pipeline(pipeline);
        free_pipeline(head);
        return NULL;
    }
    if (!command_empty(cmd))
        append_command(pipeline, cmd);
    else
        free_commands(cmd);
    if (!pipeline->commands) {
        free(pipeline);
        if (last_connector == TOK_AND || last_connector == TOK_OR) {
            set_error(error,
                "syntax error: conditional operator needs a following pipeline");
            free_pipeline(head);
            return NULL;
        }
        if (last_connector == TOK_SEQ && tail)
            tail->next_op = CONN_NONE;
        return head;
    }
    append_pipeline(&head, &tail, pipeline);
    return head;
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

static size_t count_pipelines(t_pipeline *pipeline) {
    size_t count;

    count = 0;
    while (pipeline) {
        count++;
        pipeline = pipeline->next;
    }
    return count;
}

void shell_sequence_init(t_sequence *sequence) {
    if (!sequence)
        return;
    sequence->pipelines = NULL;
    sequence->pipeline_count = 0;
}

void shell_sequence_free(t_sequence *sequence) {
    if (!sequence)
        return;
    free_pipeline(sequence->pipelines);
    sequence->pipelines = NULL;
    sequence->pipeline_count = 0;
}

int shell_parse_line(const char *line, t_sequence *sequence, char **error) {
    t_token *tokens;

    if (!sequence) {
        if (error)
            *error = sh_strdup("parse output is null");
        return 1;
    }
    shell_sequence_init(sequence);
    tokens = tokenize_line(line, error);
    if (error && *error)
        return 1;
    sequence->pipelines = parse_tokens(tokens, error);
    free_tokens(tokens);
    if (error && *error) {
        shell_sequence_free(sequence);
        return 1;
    }
    sequence->pipeline_count = count_pipelines(sequence->pipelines);
    return 0;
}

int shell_execute_sequence(const t_sequence *sequence, t_env *env,
        int *last_status, const t_executor_hooks *hooks, void *ctx) {
    const t_pipeline *pipeline;
    t_connector gate;
    int status;
    int should_run;

    if (!hooks || !hooks->run_pipeline) {
        if (hooks && hooks->on_error)
            hooks->on_error("missing executor pipeline hook", ctx);
        if (last_status)
            *last_status = 1;
        return 1;
    }
    status = last_status ? *last_status : 0;
    gate = CONN_NONE;
    pipeline = sequence ? sequence->pipelines : NULL;
    while (pipeline) {
        should_run = 1;
        if (gate == CONN_AND && status != 0)
            should_run = 0;
        if (gate == CONN_OR && status == 0)
            should_run = 0;
        if (should_run)
            status = hooks->run_pipeline(pipeline, env, ctx);
        gate = pipeline->next_op;
        pipeline = pipeline->next;
    }
    if (last_status)
        *last_status = status;
    return status;
}
