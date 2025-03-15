#ifndef SHELL_H
# define SHELL_H

# include <stddef.h>

typedef struct s_shell {
    int running;
}   t_shell;

char    *shell_read_line(const char *prompt, int interactive);
void    shell_loop(t_shell *shell);
char    *sh_strdup(const char *s);
char    *sh_substr(const char *s, size_t start, size_t len);
char    *sh_strjoin_free(char *left, const char *right);
void    *sh_xcalloc(size_t count, size_t size);
void    sh_free_words(char **words);
char    *shell_strndup(const char *s, size_t len);
void    shell_strv_free(char **words);

#endif
