#include "shell.h"
#include "string_builder.h"

#include <stdio.h>
#include <stdlib.h>

// [INTV:ARCH] token.c가 작은따옴표 문자 앞에 심어둔 sentinel — 정의를 파일마다 따로 갖고
// 있는 대신 매직 넘버 '\001'을 그대로 반복한다(TRADE_OFF: shell.h에 공용 매크로로 뽑을 수도
// 있었지만, 이 인코딩은 tokenize -> expand 두 단계 사이의 내부 계약이라 외부에 노출할 필요가
// 없다고 보고 각 파일에 지역적으로 둠).
#define LITERAL_MARK '\001'

static int append_status(t_string_builder *out, int status)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "%d", status);
    return string_builder_append_text(out, buf);
}

// [INTV:ARCH] word 문자열을 한 글자씩 훑으며 세 가지 경우를 구분한다: LITERAL_MARK로
// 시작하면(작은따옴표 안이었던 문자) 확장하지 않고 그대로 복사, `$?`는 종료 상태로, `$NAME`은
// 환경변수 값으로 치환한다. 그 외 나머지 문자는 그대로 둔다 — 이 순서(LITERAL_MARK 체크가
// $ 체크보다 먼저)가 중요하다.
// - [TRAP] LITERAL_MARK 체크를 뒤로 미루면 작은따옴표로 감싼 '$HOME' 안의 '$'까지 변수로
//   확장돼버린다 — 작은따옴표가 확장을 막는다는 셸의 기본 규칙이 깨진다.
char *expand_word(t_shell *shell, const char *word)
{
    t_string_builder    out;
    size_t              i;

    if (string_builder_init(&out) != 0)
        return NULL;
    i = 0;
    while (word != NULL && word[i] != '\0') {
        int failed;

        failed = 0;
        if (word[i] == LITERAL_MARK && word[i + 1] != '\0') {
            failed = string_builder_append_char(&out, word[i + 1]);
            i += 2;
        } else if (word[i] == '$' && word[i + 1] == '?') {
            failed = append_status(&out, shell->last_status);
            i += 2;
        } else if (word[i] == '$'
            && sh_is_name_start((unsigned char)word[i + 1])) {
            size_t  start;
            char    *key;

            start = i + 1;
            i = start + 1;
            while (sh_is_name_char((unsigned char)word[i]))
                i++;
            key = sh_substr(word, start, i - start);
            if (key == NULL) {
                string_builder_discard(&out);
                return NULL;
            }
            // [INTV:EDGE] env_get()은 없는 변수에 대해 NULL이 아니라 ""(빈 문자열)를 돌려준다
            // — bash가 미설정 변수를 빈 문자열로 치환하는 것과 동일한 동작. 여기서 NULL 체크를
            // 따로 하지 않는 이유이기도 하다.
            failed = string_builder_append_text(&out,
                    env_get(shell->env, key));
            free(key);
        } else {
            failed = string_builder_append_char(&out, word[i]);
            i++;
        }
        if (failed != 0) {
            string_builder_discard(&out);
            return NULL;
        }
    }
    return string_builder_take(&out);
}

// [INTV:ARCH] expand_word()와 달리 $변수 치환은 하지 않고 LITERAL_MARK만 벗겨낸다 — 히어독
// 구분자(delimiter)처럼 "따옴표 여부만 원문 그대로 복원하면 되고 변수 확장은 하면 안 되는"
// 대상에 쓴다(아래 shell_dequote_word, heredoc.c의 REDIR_HEREDOC 타깃 처리 참고).
static char *dequote_word(const char *word)
{
    t_string_builder    out;
    size_t              i;

    if (string_builder_init(&out) != 0)
        return NULL;
    i = 0;
    while (word != NULL && word[i] != '\0') {
        int failed;

        if (word[i] == LITERAL_MARK && word[i + 1] != '\0') {
            failed = string_builder_append_char(&out, word[i + 1]);
            i += 2;
        } else {
            failed = string_builder_append_char(&out, word[i]);
            i++;
        }
        if (failed != 0) {
            string_builder_discard(&out);
            return NULL;
        }
    }
    return string_builder_take(&out);
}

int shell_dequote_word(const char *word, char **out, char **error)
{
    if (error != NULL)
        *error = NULL;
    if (out == NULL) {
        if (error != NULL)
            *error = sh_strdup("dequote output is null");
        return 1;
    }
    *out = dequote_word(word);
    if (*out == NULL) {
        if (error != NULL)
            *error = sh_strdup("allocation failure");
        return 1;
    }
    return 0;
}

static int expand_words(t_shell *shell, char ***words)
{
    size_t  i;
    char    *expanded;

    i = 0;
    while (*words != NULL && (*words)[i] != NULL) {
        expanded = expand_word(shell, (*words)[i]);
        if (expanded == NULL)
            return 1;
        free((*words)[i]);
        (*words)[i] = expanded;
        i++;
    }
    return 0;
}

// [INTV:ARCH] 히어독 리다이렉션(REDIR_HEREDOC)의 타깃은 인자/일반 리다이렉션과 다른 확장
// 규칙을 쓴다 — 인자와 파일 경로는 expand_word()로 $변수를 치환하지만, 히어독 타깃은
// 구분자(delimiter) 문자열 그 자체이므로 dequote_word()로 따옴표만 벗기고 변수 확장은
// 하지 않는다.
// - [TRAP] 여기서 expand_word()를 잘못 썼다면 delimiter 문자열 안의 `$`가 먼저 확장돼
//   버려서, heredoc.c가 실제 입력 종료를 찾을 때 쓰는 delimiter 값이 사용자가 타이핑한
//   것과 달라진다(대개는 문제되지 않지만 delimiter에 `$`가 들어간 특수한 경우 깨진다).
int expand_pipeline(t_shell *shell, t_pipeline *pipeline)
{
    t_command   *cmd;
    t_redir     *redir;
    char        *expanded;

    while (pipeline != NULL) {
        cmd = pipeline->commands;
        while (cmd != NULL) {
            if (expand_words(shell, &cmd->argv) != 0)
                return 1;
            redir = cmd->redirs;
            while (redir != NULL) {
                if (redir->type == REDIR_HEREDOC)
                    expanded = dequote_word(redir->target);
                else
                    expanded = expand_word(shell, redir->target);
                if (expanded == NULL)
                    return 1;
                free(redir->target);
                redir->target = expanded;
                redir = redir->next;
            }
            cmd = cmd->next;
        }
        pipeline = pipeline->next;
    }
    return 0;
}

int shell_expand_sequence(t_sequence *sequence, const t_env *env,
        int last_status, char **error)
{
    t_shell shell;
    int     result;

    if (error != NULL)
        *error = NULL;
    if (sequence == NULL)
        return 0;
    shell.env = (t_env *)env;
    shell.last_status = last_status;
    shell.running = 1;
    result = expand_pipeline(&shell, sequence->pipelines);
    if (result != 0 && error != NULL)
        *error = sh_strdup("allocation failure");
    return result;
}
