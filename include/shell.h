#ifndef SHELL_H
# define SHELL_H

# include <stddef.h>

typedef enum e_token_type {
    TOK_WORD,
    TOK_PIPE,
    TOK_REDIR_IN,
    TOK_REDIR_OUT,
    TOK_REDIR_APPEND,
    TOK_AND,
    TOK_OR,
    TOK_SEQ
}   t_token_type;

typedef struct s_token {
    t_token_type    type;
    char            *text;
    size_t          start;
    struct s_token  *next;
}   t_token;

typedef enum e_redir_type {
    REDIR_IN,
    REDIR_OUT,
    REDIR_APPEND
}   t_redir_type;

# define SHELL_REDIR_IN REDIR_IN
# define SHELL_REDIR_OUT REDIR_OUT
# define SHELL_REDIR_APPEND REDIR_APPEND

typedef struct s_redir {
    t_redir_type    type;
    char            *target;
    struct s_redir  *next;
}   t_redir;

typedef struct s_command {
    char                **argv;
    size_t              argc;
    t_redir             *redirs;
    struct s_command    *next;
}   t_command;

typedef enum e_connector {
    CONN_NONE,
    CONN_SEQ,
    CONN_AND,
    CONN_OR
}   t_connector;

typedef struct s_pipeline {
    t_command           *commands;
    size_t              command_count;
    t_connector         next_op;
    struct s_pipeline   *next;
}   t_pipeline;

typedef struct s_sequence {
    t_pipeline  *pipelines;
    size_t      pipeline_count;
}   t_sequence;

typedef struct s_env {
    char            *key;
    char            *value;
    int             exported;
    struct s_env    *next;
}   t_env;

typedef struct s_shell {
    t_env   *env;
    int     last_status;
    int     running;
}   t_shell;

typedef int     (*t_shell_run_pipeline_fn)(const t_pipeline *pipeline,
                    t_env *env, void *ctx);
typedef void    (*t_shell_error_fn)(const char *message, void *ctx);

typedef struct s_executor_hooks {
    t_shell_run_pipeline_fn run_pipeline;
    t_shell_error_fn        on_error;
}   t_executor_hooks;

char    *shell_read_line(const char *prompt, int interactive);
void    shell_loop(t_shell *shell);
char    *sh_strdup(const char *s);
char    *sh_substr(const char *s, size_t start, size_t len);
char    *sh_strjoin_free(char *left, const char *right);
void    *sh_xcalloc(size_t count, size_t size);
void    sh_free_words(char **words);
char    *shell_strndup(const char *s, size_t len);
char    *shell_itoa_status(int status);
void    shell_strv_free(char **words);
int     sh_is_name_char(int c);
int     sh_is_name_start(int c);
t_token *tokenize_line(const char *line, char **error);
void    free_tokens(t_token *tokens);
t_pipeline  *parse_tokens(t_token *tokens, char **error);
void    free_pipeline(t_pipeline *pipeline);
void    shell_sequence_init(t_sequence *sequence);
void    shell_sequence_free(t_sequence *sequence);
int     shell_parse_line(const char *line, t_sequence *sequence, char **error);
int     shell_execute_sequence(const t_sequence *sequence, t_env *env,
            int *last_status, const t_executor_hooks *hooks, void *ctx);
char    *expand_word(t_shell *shell, const char *word);
int     expand_pipeline(t_shell *shell, t_pipeline *pipeline);
int     shell_dequote_word(const char *word, char **out, char **error);
int     shell_expand_sequence(t_sequence *sequence, const t_env *env,
            int last_status, char **error);
t_env   *env_from_environ(char **envp);
void    env_free(t_env *env);
const char  *env_get(t_env *env, const char *key);
int     env_set(t_env **env, const char *key, const char *value, int exported);
int     env_unset(t_env **env, const char *key);
char    **env_to_environ(t_env *env);
void    env_print(t_env *env, int declare_style);
int     shell_env_init(t_env *env, char **envp);
void    shell_env_free(t_env *env);
const char  *shell_env_get(const t_env *env, const char *key);
int     shell_env_set(t_env *env, const char *key, const char *value,
            int exported);
int     shell_env_unset(t_env *env, const char *key);
int     shell_env_is_valid_name(const char *key);
char    **shell_env_export_list(t_env *env);
char    **shell_env_to_envp(t_env *env);
int     builtin_is_parent(const char *name);
int     builtin_is_known(const char *name);
int     builtin_run(t_shell *shell, char **argv);
int     execute_pipeline_list(t_shell *shell, t_pipeline *pipeline);
int     shell_process_line(t_shell *shell, const char *line);

#endif
