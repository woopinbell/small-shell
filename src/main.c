#define _POSIX_C_SOURCE 200809L

#include "shell.h"

int main(int argc, char **argv)
{
    t_shell shell;

    (void)argc;
    (void)argv;
    shell.running = 1;
    shell_loop(&shell);
    return 0;
}
