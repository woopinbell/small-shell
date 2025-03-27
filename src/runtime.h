#ifndef RUNTIME_H
# define RUNTIME_H

# include <sys/types.h>

int     shell_pipe(int fds[2]);
pid_t   shell_fork(void);
pid_t   shell_waitpid(pid_t pid, int *status, int options);

#endif
