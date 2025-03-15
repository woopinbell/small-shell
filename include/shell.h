#ifndef SHELL_H
# define SHELL_H

typedef struct s_shell {
    int running;
}   t_shell;

char    *shell_read_line(const char *prompt, int interactive);
void    shell_loop(t_shell *shell);

#endif
