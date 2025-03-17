#ifndef SHELL_H
# define SHELL_H

# include <stddef.h>

typedef enum e_token_type {
    TOK_WORD
}   t_token_type;

typedef struct s_token {
    t_token_type    type;
    char            *text;
    size_t          start;
    struct s_token  *next;
}   t_token;

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

#endif
