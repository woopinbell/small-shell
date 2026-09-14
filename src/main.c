#define _POSIX_C_SOURCE 200809L

#include "shell.h"
#include "runtime.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

// [INTV:EDGE] 종료 코드는 POSIX 관례상 하위 1바이트(0~255)만 의미가 있다. exec.c의
// process_error_status()가 세션 내부에서만 쓰는 258(범위 밖 값)을 last_status에 넣어두는데,
// 그게 실제 프로세스 종료 코드로 나가기 직전인 여기서만 0xff로 마스킹한다 — $? 조회 시점에는
// 마스킹하지 않아야 258을 파싱 에러 전용 신호로 구분할 수 있기 때문(아래 shell->last_status
// 대입부, exec.c 참고).
static int normalize_status(int status)
{
    return status & 0xff;
}

int main(int argc, char **argv, char **envp)
{
    t_shell shell;
    int result;

    (void)argc;
    (void)argv;
    errno = 0;
    shell_runtime_set_alloc_scope("startup");
    shell.env = env_from_environ(envp);
    // [INTV:TRAP] env_from_environ()이 NULL을 반환하는 경우가 두 가지라 errno로 구분해야 한다:
    // (1) envp가 비어 있어서 정상적으로 빈 리스트가 만들어진 경우, (2) 도중에 calloc/strdup이
    // 실패한 경우. envp[0] != NULL(즉 원래 환경변수가 있었는데도 NULL이 나왔다)이면서
    // errno == ENOMEM일 때만 진짜 실패로 취급한다 — 이 구분 없이 그냥 "NULL이면 실패"로
    // 처리하면 빈 환경에서 부팅할 때마다 멀쩡한 셸이 즉시 죽는다.
    if (shell.env == NULL && envp != NULL && envp[0] != NULL
        && errno == ENOMEM) {
        fprintf(stderr, "small-shell: startup: %s\n", strerror(errno));
        return 1;
    }
    shell.last_status = 0;
    shell.running = 1;

    shell_loop(&shell);
    result = shell.last_status;
    env_free(shell.env);
    return normalize_status(result);
}
