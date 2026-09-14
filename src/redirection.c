#define _POSIX_C_SOURCE 200809L

#include "exec_internal.h"
#include "runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int heredoc_stream_error(FILE *stream, const char *operation)
{
    int saved_errno;

    saved_errno = errno;
    if (saved_errno == 0)
        saved_errno = EIO;
    fprintf(stderr, "small-shell: heredoc %s: %s\n", operation,
        strerror(saved_errno));
    fclose(stream);
    errno = saved_errno;
    return 1;
}

// [INTV:ARCH] 리다이렉션은 command->redirs 리스트를 순서대로 하나씩 적용한다 — 같은 방향
// 리다이렉트가 여러 번 있으면(parser.c의 add_redir 주석 참고) 뒤에 열린 fd가 dup2로 앞
// 것을 덮어써서 "마지막 리다이렉션이 이긴다"가 자연히 성립한다.
int exec_apply_redirections(const t_command *command,
    const struct exec_context *ctx)
{
    const t_redir *redir;

    redir = command->redirs;
    while (redir != NULL) {
        int fd;

        if (redir->type == REDIR_IN) {
            fd = shell_open(redir->target, O_RDONLY, 0);
            if (fd < 0) {
                fprintf(stderr, "small-shell: %s: %s\n", redir->target,
                    strerror(errno));
                return 1;
            }
            if (shell_dup2(fd, STDIN_FILENO) < 0) {
                fprintf(stderr, "small-shell: dup2: %s\n", strerror(errno));
                close(fd);
                return 1;
            }
            close(fd);
        } else if (redir->type == REDIR_OUT
            || redir->type == REDIR_APPEND) {
            int flags;

            flags = O_WRONLY | O_CREAT;
            if (redir->type == REDIR_OUT)
                flags |= O_TRUNC;
            else
                flags |= O_APPEND;
            fd = shell_open(redir->target, flags, 0644);
            if (fd < 0) {
                fprintf(stderr, "small-shell: %s: %s\n", redir->target,
                    strerror(errno));
                return 1;
            }
            if (shell_dup2(fd, STDOUT_FILENO) < 0) {
                fprintf(stderr, "small-shell: dup2: %s\n", strerror(errno));
                close(fd);
                return 1;
            }
            close(fd);
        } else if (redir->type == REDIR_HEREDOC) {
            FILE        *tmp;
            const char  *body;

            // [INTV:ARCH] 히어독 본문은 파이프가 아니라 tmpfile()로 만든 실제(익명) 파일에
            // 써놓고 그 fd를 stdin으로 dup2한다 — 파이프로 하면 본문이 파이프 버퍼 크기(보통
            // 64KB)를 넘을 때 쓰는 쪽(셸)과 읽는 쪽(자식)이 서로 막힐 수 있는 반면, 임시
            // 파일은 크기 제한 없이 한 번에 다 쓰고 lseek로 되감아 읽게 해 그 교착 가능성을
            // 없앤다.
            tmp = tmpfile();
            if (tmp == NULL) {
                fprintf(stderr, "small-shell: heredoc: %s\n",
                    strerror(errno));
                return 1;
            }
            body = exec_find_heredoc_body(ctx, redir);
            if (body != NULL && fputs(body, tmp) == EOF)
                return heredoc_stream_error(tmp, "write");
            if (shell_fflush(tmp) != 0)
                return heredoc_stream_error(tmp, "flush");
            if (shell_fseek(tmp, 0L, SEEK_SET) != 0)
                return heredoc_stream_error(tmp, "seek");
            fd = shell_fileno(tmp);
            if (fd < 0)
                return heredoc_stream_error(tmp, "descriptor");
            if (shell_dup2(fd, STDIN_FILENO) < 0) {
                fprintf(stderr, "small-shell: dup2: %s\n", strerror(errno));
                fclose(tmp);
                return 1;
            }
            fclose(tmp);
        }
        redir = redir->next;
    }
    return 0;
}

static int save_stdio(int saved[2])
{
    saved[0] = shell_dup(STDIN_FILENO);
    saved[1] = shell_dup(STDOUT_FILENO);
    if (saved[0] < 0 || saved[1] < 0) {
        if (saved[0] >= 0)
            close(saved[0]);
        if (saved[1] >= 0)
            close(saved[1]);
        fprintf(stderr, "small-shell: dup: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

static int restore_one(int saved, int target)
{
    int attempts;
    int had_error;

    attempts = 0;
    had_error = 0;
    while (attempts < 2) {
        if (shell_dup2(saved, target) >= 0)
            return had_error;
        if (errno == EINTR)
            continue;
        fprintf(stderr, "small-shell: dup2: %s\n", strerror(errno));
        had_error = 1;
        attempts++;
    }
    return -1;
}

static int restore_stdio(int saved[2])
{
    int input_result;
    int output_result;

    input_result = restore_one(saved[0], STDIN_FILENO);
    output_result = restore_one(saved[1], STDOUT_FILENO);
    close(saved[0]);
    close(saved[1]);
    if (input_result < 0 || output_result < 0)
        return -1;
    return (input_result != 0 || output_result != 0);
}

// [INTV:ARCH] 부모(셸) 프로세스 안에서 직접 실행하는 빌트인(cd 등)에 리다이렉션이 걸리면
// (`cd /tmp > out.txt`), fork 없이 실행하는 대신 fd 0/1을 dup으로 백업했다가 실행 후 복원한다.
// - [TRAP] 백업/복원 없이 그냥 dup2로 리다이렉션만 걸면, 그 빌트인 실행이 끝난 뒤에도 셸
//   자신의 stdin/stdout이 리다이렉션 대상 파일에 계속 연결된 채로 남아 이후 모든 명령의
//   입출력이 그 파일로 새는 치명적인 상태가 된다 — 자식 프로세스라면 exit로 자연히
//   사라졌을 변경이, 부모에서는 명시적으로 되돌려야 한다.
int exec_run_parent_command(t_shell *shell, const t_command *command,
    const struct exec_context *ctx)
{
    int saved[2];
    int status;

    if (save_stdio(saved) != 0)
        return 1;
    if (exec_apply_redirections(command, ctx) != 0) {
        if (restore_stdio(saved) < 0)
            shell->running = 0;
        return 1;
    }
    if (command->argc == 0)
        status = 0;
    else
        status = builtin_run(shell, command->argv);
    {
        int restore_result;

        restore_result = restore_stdio(saved);
        if (restore_result != 0)
            status = 1;
        // [INTV:TRAP] stdin/stdout 복원 자체가 실패하면(위 restore_one이 -1을 반환) 셸을
        // 계속 돌리지 않고 running = 0으로 끈다 — fd 상태가 무엇을 가리키는지 알 수 없는
        // 채로 다음 명령을 계속 처리하면, 이후 모든 입출력이 어디로 갈지 예측할 수 없는
        // 상태가 되어 그냥 죽는 것보다 위험하다.
        if (restore_result < 0)
            shell->running = 0;
    }
    return status;
}
