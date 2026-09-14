#define _POSIX_C_SOURCE 200809L

#include "shell.h"
#include "runtime.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

// [INTV:TRADE_OFF] readline이 없는 환경(테스트 빌드, readline 미설치 시스템)에서도 셸이
// 동작해야 해서, 한 글자씩 read(2)로 직접 읽어 줄을 조립하는 이 폴백을 따로 구현했다 —
// 그 대가로 방향키 히스토리·라인 편집 같은 readline의 UX는 없다.
static char *read_plain_line(const char *prompt, int interactive, int *failed)
{
    size_t  cap;
    size_t  len;
    char    *line;

    *failed = 0;
    // [INTV:ARCH] 프롬프트는 stdout이 아니라 stderr로 쓴다 — 파이프/리다이렉트로 stdout이
    // 다른 곳으로 연결돼 있어도 프롬프트가 항상 터미널에 보이게 하기 위함이며, 동시에
    // "echo | other-shell" 식으로 출력이 파싱되는 상황에서 프롬프트 문자열이 데이터에
    // 섞이지 않게 한다.
    if (interactive && prompt != NULL
        && shell_write_text(STDERR_FILENO, prompt) != 0) {
        *failed = 1;
        return NULL;
    }
    cap = 128;
    len = 0;
    line = (char *)shell_malloc(cap);
    if (line == NULL) {
        *failed = 1;
        return NULL;
    }
    for (;;) {
        unsigned char   ch;
        ssize_t         count;

        count = shell_read(STDIN_FILENO, &ch, 1);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0) {
            free(line);
            *failed = 1;
            return NULL;
        }
        if (count == 0) {
            // [INTV:EDGE] EOF(Ctrl-D)가 왔을 때 그때까지 읽은 내용이 있으면(len != 0) 그
            // 줄은 그대로 완성해서 돌려준다 — bash도 "마지막 줄에 개행이 없는 입력"을
            // 버리지 않고 처리한다. 아무것도 못 읽은 상태에서의 EOF만 "입력 끝"으로 취급.
            if (len == 0) {
                free(line);
                return NULL;
            }
            break;
        }
        if (ch == '\n')
            break;
        if (len + 1 >= cap) {
            char *grown;

            // [INTV:TRAP] cap *= 2 전에 cap > SIZE_MAX / 2로 오버플로를 미리 검사한다 —
            // 검사 없이 배로 늘리면 cap이 SIZE_MAX를 넘어 0 근처로 wrap되고, 그 작은 값으로
            // realloc한 버퍼에 계속 쓰면서 힙을 깨뜨린다.
            if (cap > SIZE_MAX / 2) {
                free(line);
                errno = ENOMEM;
                *failed = 1;
                return NULL;
            }
            cap *= 2;
            grown = (char *)shell_realloc(line, cap);
            if (grown == NULL) {
                free(line);
                *failed = 1;
                return NULL;
            }
            line = grown;
        }
        line[len++] = (char)ch;
    }
    line[len] = '\0';
    return line;
}

char *shell_read_line(const char *prompt, int interactive, int *failed)
{
#ifdef USE_READLINE
    if (interactive) {
        char *line;

        *failed = 0;
        line = readline(prompt != NULL ? prompt : "");
        // [INTV:EDGE] readline은 실패와 "빈 줄 입력"을 둘 다 다르게 반환한다: 실제 EOF는
        // NULL, 빈 줄은 길이 0인 문자열. 빈 줄까지 히스토리에 넣으면 방향키로 올렸을 때
        // 의미 없는 빈 항목이 계속 나오므로 line[0] != '\0'일 때만 add_history한다.
        if (line != NULL && line[0] != '\0')
            add_history(line);
        return line;
    }
#endif
    return read_plain_line(prompt, interactive, failed);
}

void shell_loop(t_shell *shell)
{
    int     interactive;
    char    *line;

    if (shell == NULL)
        return;
    // [INTV:EDGE] stdin뿐 아니라 stderr까지 둘 다 tty여야 interactive로 판단한다 — 프롬프트를
    // stderr로 쓰기 때문에(위 read_plain_line 참고), stdin만 tty고 stderr가 파일로 리다이렉트된
    // 상태라면 프롬프트가 화면에 안 보이는데도 해당 프롬프트 출력 시도 자체는 의미가 있어야
    // 하므로 둘 다 검사해 "사람이 실제로 보고 입력하는 상황"만 interactive로 취급한다.
    interactive = isatty(STDIN_FILENO) && isatty(STDERR_FILENO);
    while (shell->running) {
        int failed;

        shell_runtime_set_alloc_scope("command-input");
        line = shell_read_line("small-shell$ ", interactive, &failed);
        if (line == NULL) {
            if (failed) {
                fprintf(stderr, "small-shell: input: %s\n", strerror(errno));
                shell->last_status = 1;
            }
            break;
        }
        (void)shell_process_line(shell, line);
        free(line);
    }
    if (interactive && shell->running)
        (void)shell_write_text(STDERR_FILENO, "\n");
}
