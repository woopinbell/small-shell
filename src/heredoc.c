#define _POSIX_C_SOURCE 200809L

#include "exec_internal.h"
#include "runtime.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LITERAL_MARK '\001'

struct strbuf {
    char    *data;
    size_t  len;
    size_t  cap;
};

char *shell_read_line(const char *prompt, int interactive, int *failed);

static int sb_init(struct strbuf *buf)
{
    buf->cap = 64;
    buf->len = 0;
    buf->data = (char *)shell_malloc(buf->cap);
    if (buf->data == NULL)
        return 1;
    buf->data[0] = '\0';
    return 0;
}

static void sb_free(struct strbuf *buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

static int sb_reserve(struct strbuf *buf, size_t extra)
{
    size_t  needed;
    char    *next;

    needed = buf->len + extra + 1;
    if (needed <= buf->cap)
        return 0;
    while (buf->cap < needed) {
        if (buf->cap > SIZE_MAX / 2)
            return 1;
        buf->cap *= 2;
    }
    next = (char *)shell_realloc(buf->data, buf->cap);
    if (next == NULL)
        return 1;
    buf->data = next;
    return 0;
}

static int sb_push(struct strbuf *buf, char ch)
{
    if (sb_reserve(buf, 1) != 0)
        return 1;
    buf->data[buf->len++] = ch;
    buf->data[buf->len] = '\0';
    return 0;
}

static int sb_append(struct strbuf *buf, const char *text)
{
    size_t len;

    if (text == NULL)
        return 0;
    len = strlen(text);
    if (sb_reserve(buf, len) != 0)
        return 1;
    memcpy(buf->data + buf->len, text, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
    return 0;
}

static int append_status(struct strbuf *buf, int status)
{
    char tmp[32];

    snprintf(tmp, sizeof(tmp), "%d", status);
    return sb_append(buf, tmp);
}

static int expand_dollar_at(t_shell *shell, const char *line, size_t *i,
    struct strbuf *out)
{
    size_t pos;

    pos = *i;
    if (line[pos + 1] == '?') {
        *i = pos + 2;
        return append_status(out, shell->last_status);
    }
    if (sh_is_name_start((unsigned char)line[pos + 1])) {
        size_t      start;
        size_t      end;
        char        *name;
        const char  *value;
        int         result;

        start = pos + 1;
        end = start + 1;
        while (sh_is_name_char((unsigned char)line[end]))
            end++;
        name = shell_strndup(line + start, end - start);
        if (name == NULL)
            return 1;
        value = env_get(shell->env, name);
        result = sb_append(out, value);
        free(name);
        *i = end;
        return result;
    }
    *i = pos + 1;
    return sb_push(out, '$');
}

static int expand_heredoc_body_line(t_shell *shell, const char *line,
    struct strbuf *out)
{
    size_t i;

    i = 0;
    while (line[i] != '\0') {
        if (line[i] == '$') {
            if (expand_dollar_at(shell, line, &i, out) != 0)
                return 1;
        } else {
            if (sb_push(out, line[i]) != 0)
                return 1;
            i++;
        }
    }
    return 0;
}

// [INTV:ARCH] expand.c에 있는 dequote_word()와 같은 일(LITERAL_MARK 제거)을 하는 별도
// 구현이다 — 코드를 공유하지 않고 이 파일 안에 복제해둔 이유는 exec_internal.h 쪽 구조체
// (struct strbuf)와 string_builder.h 쪽 t_string_builder가 서로 다른 버퍼 타입이라, 공용
// 함수로 뽑으려면 둘 중 하나에 의존성을 추가해야 했기 때문이다. 히어독 처리는 exec 계층
// 내부에 완전히 갇혀 있게 하려고 이 정도 중복은 감수했다.
static char *dequote_runtime_word(const char *word)
{
    struct strbuf   out;
    size_t          i;

    if (sb_init(&out) != 0)
        return NULL;
    i = 0;
    while (word != NULL && word[i] != '\0') {
        if (word[i] == LITERAL_MARK && word[i + 1] != '\0') {
            if (sb_push(&out, word[i + 1]) != 0) {
                sb_free(&out);
                return NULL;
            }
            i += 2;
        } else {
            if (sb_push(&out, word[i]) != 0) {
                sb_free(&out);
                return NULL;
            }
            i++;
        }
    }
    return out.data;
}

static int delimiter_matches(const char *line, const char *encoded)
{
    size_t i;
    size_t j;

    i = 0;
    j = 0;
    while (encoded != NULL && encoded[i] != '\0') {
        if (encoded[i] == LITERAL_MARK && encoded[i + 1] != '\0')
            i++;
        if (line[j] != encoded[i])
            return 0;
        i++;
        j++;
    }
    return line[j] == '\0';
}

// [INTV:ARCH] 히어독 하나를 읽다가 실패(alloc 실패 등)해도, 그 히어독의 구분자가 나올
// 때까지는 입력을 계속 읽어서 버린다.
// - [TRAP] 이 소비 없이 바로 리턴해버리면, 사용자가 터미널에 이어서 입력한 히어독 본문
//   줄들이 다음 명령으로 잘못 해석된다 — 실패한 히어독의 "나머지 줄"이 셸 프롬프트에
//   명령어처럼 먹혀 들어가는 것.
static int discard_heredoc(const char *delimiter, int interactive)
{
    for (;;) {
        char    *line;
        int     failed;

        shell_runtime_set_alloc_scope("input");
        line = shell_read_line("> ", interactive, &failed);
        if (line == NULL)
            return failed;
        if (delimiter_matches(line, delimiter)) {
            free(line);
            return 0;
        }
        free(line);
    }
}

static int append_heredoc_body_line(t_shell *shell, int quoted,
    struct strbuf *body, const char *line)
{
    // [INTV:ARCH] 구분자가 따옴표로 감싸여 있었는지(`<< "EOF"` 등)에 따라 본문 안의 `$변수`를
    // 확장할지 말지가 갈린다(POSIX 히어독 규칙) — quoted면 원문 그대로, 아니면 줄 단위로
    // $ 확장을 수행한다.
    if (quoted) {
        if (sb_append(body, line) != 0)
            return 1;
    } else if (expand_heredoc_body_line(shell, line, body) != 0) {
        return 1;
    }
    return sb_push(body, '\n');
}

static int add_heredoc_entry(struct exec_context *ctx, const t_redir *redir,
    char *body)
{
    struct heredoc_entry *entry;

    entry = (struct heredoc_entry *)shell_malloc(sizeof(*entry));
    if (entry == NULL)
        return 1;
    entry->redir = redir;
    entry->body = body;
    entry->next = ctx->heredocs;
    ctx->heredocs = entry;
    return 0;
}

void exec_heredoc_entries_free(struct heredoc_entry *entry)
{
    struct heredoc_entry *next;

    while (entry != NULL) {
        next = entry->next;
        free(entry->body);
        free(entry);
        entry = next;
    }
}

// [INTV:ARCH] 히어독 본문은 redir 노드의 포인터 주소를 key 삼아 별도 리스트(ctx->heredocs)에
// 저장해뒀다가, 나중에 실제 리다이렉션을 적용할 때(redirection.c) 이 key로 다시 찾아온다.
// t_redir 구조체 자체에 body 필드를 추가하지 않은 이유는 t_redir이 파서 계층의 순수 데이터
// 구조라, 실행 계층에서만 쓰는 상태(읽어들인 본문)를 거기 얹으면 파서와 실행기 사이의
// 경계가 흐려지기 때문이다.
const char *exec_find_heredoc_body(const struct exec_context *ctx,
    const t_redir *redir)
{
    struct heredoc_entry *entry;

    entry = ctx->heredocs;
    while (entry != NULL) {
        if (entry->redir == redir)
            return entry->body;
        entry = entry->next;
    }
    return "";
}

// [INTV:ARCH] - [TRAP] 히어독은 실제로 명령을 fork/exec 하기 훨씬 전, 파이프라인 전체를
// 실행하기 직전에 한꺼번에 미리 읽어둔다(아래 exec_prepare_heredocs 참고). 만약 각 자식
// 프로세스가 fork된 후에 자기 히어독을 직접 읽으려 했다면, 파이프라인 안에 히어독이 여러 개
// 있을 때 여러 자식이 동시에 같은 부모 stdin을 놓고 경쟁하게 되어 어느 히어독이 어느 입력을
// 가져갈지 보장할 수 없다 — 부모(셸) 프로세스 하나가 터미널을 갖고 있을 때 순차적으로 미리
// 읽어 자식에게 넘겨주는 방식으로 이 경쟁을 원천 차단한다.
static int read_heredoc(struct exec_context *ctx, t_redir *redir)
{
    struct strbuf   body;
    char            *delimiter;
    int             quoted;
    int             interactive;
    int             input_failed;

    interactive = isatty(STDIN_FILENO) && isatty(STDERR_FILENO);
    shell_runtime_set_alloc_scope("heredoc");
    quoted = redir->heredoc_quoted;
    delimiter = dequote_runtime_word(redir->target);
    if (delimiter == NULL) {
        (void)discard_heredoc(redir->target, interactive);
        return 1;
    }
    free(redir->target);
    redir->target = delimiter;
    if (sb_init(&body) != 0) {
        (void)discard_heredoc(redir->target, interactive);
        return 1;
    }
    for (;;) {
        char *line;

        shell_runtime_set_alloc_scope("input");
        line = shell_read_line("> ", interactive, &input_failed);
        if (line == NULL) {
            if (input_failed) {
                fprintf(stderr, "small-shell: heredoc input: %s\n",
                    strerror(errno));
                if (discard_heredoc(redir->target, interactive) != 0)
                    ctx->shell->running = 0;
                sb_free(&body);
                return 1;
            }
            // [INTV:EDGE] 구분자를 못 만난 채로 입력이 EOF에 도달하면 에러로 죽이지 않고
            // bash와 동일한 문구("warning: here-document delimited by end-of-file")로 경고만
            // 남긴 뒤 그때까지 읽은 내용을 그대로 본문으로 쓴다 — 비대화형 입력(파일/파이프로
            // 스크립트를 먹일 때)에서 실수로 구분자를 안 달아도 셸 전체가 죽지 않게 한다.
            fprintf(stderr,
                "small-shell: warning: here-document delimited by end-of-file (wanted `%s')\n",
                redir->target);
            break;
        }
        if (strcmp(line, redir->target) == 0) {
            free(line);
            break;
        }
        shell_runtime_set_alloc_scope("heredoc");
        if (append_heredoc_body_line(ctx->shell, quoted, &body, line) != 0) {
            free(line);
            (void)discard_heredoc(redir->target, interactive);
            sb_free(&body);
            return 1;
        }
        free(line);
    }
    shell_runtime_set_alloc_scope("heredoc");
    if (add_heredoc_entry(ctx, redir, body.data) != 0) {
        sb_free(&body);
        return 1;
    }
    return 0;
}

int exec_prepare_heredocs(struct exec_context *ctx, t_pipeline *pipelines)
{
    t_pipeline  *pipeline;
    int         failed;
    int         interactive;

    failed = 0;
    interactive = isatty(STDIN_FILENO) && isatty(STDERR_FILENO);
    pipeline = pipelines;
    while (pipeline != NULL) {
        t_command *command;

        command = pipeline->commands;
        while (command != NULL) {
            t_redir *redir;

            redir = command->redirs;
            while (redir != NULL) {
                if (redir->type == REDIR_HEREDOC) {
                    // [INTV:TRAP] 한 히어독이 실패한 뒤에도 나머지 히어독들을 실행하지 않고
                    // 각각 discard_heredoc으로 "소비만" 한다 — 실패 이후 읽기를 계속 시도하면
                    // 이미 깨진 상태에서 또 실패할 수 있고, 그렇다고 아예 안 읽으면 뒤에 남은
                    // 히어독 본문 줄들이 다음 명령 프롬프트에 명령어처럼 잘못 먹힌다(위
                    // discard_heredoc 주석과 동일한 이유).
                    if (!failed && read_heredoc(ctx, redir) != 0)
                        failed = 1;
                    else if (failed
                        && discard_heredoc(redir->target, interactive) != 0)
                        failed = 1;
                }
                redir = redir->next;
            }
            command = command->next;
        }
        pipeline = pipeline->next;
    }
    if (failed)
        fprintf(stderr, "small-shell: heredoc: preparation failure\n");
    return failed;
}
