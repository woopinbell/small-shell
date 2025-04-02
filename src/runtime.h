#ifndef RUNTIME_H
# define RUNTIME_H

# include <stdio.h>
# include <stddef.h>
# include <sys/types.h>
# include <sys/stat.h>

void    *shell_malloc(size_t size);
void    *shell_calloc(size_t count, size_t size);
void    *shell_realloc(void *ptr, size_t size);
int     shell_pipe(int fds[2]);
pid_t   shell_fork(void);
pid_t   shell_waitpid(pid_t pid, int *status, int options);
int     shell_dup(int fd);
int     shell_dup2(int oldfd, int newfd);
int     shell_open(const char *path, int flags, mode_t mode);
int     shell_fflush(FILE *stream);
int     shell_fseek(FILE *stream, long offset, int whence);
int     shell_fileno(FILE *stream);
ssize_t shell_read(int fd, void *buffer, size_t size);

#endif
