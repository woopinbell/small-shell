#ifndef SHELL_H
# define SHELL_H

# include <stddef.h>

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
t_env   *env_from_environ(char **envp);
void    env_free(t_env *env);

#endif
