#include "shell.h"
#include "string_builder.h"

#include <stdlib.h>

// [INTV:ARCH] 토큰의 word 텍스트 안에, 작은따옴표로 감싸져서 "나중에 $변수 확장을 하면 안
// 되는" 문자를 표시할 별도 메타데이터 구조 대신, 그 문자 바로 앞에 이 sentinel 바이트를
// 끼워 넣는 인코딩을 쓴다. 셸 입력에 원래 나타날 수 없는 제어문자(\001)라서 실제 데이터와
// 충돌하지 않는다.
// - [TRAP] expand.c/heredoc.c/exec.c 세 곳 모두 이 LITERAL_MARK를 알아야 한다 — 이 마크를
//   모르고 word 문자열을 그대로 출력하거나 비교하면 \001이 섞인 채로 새어나간다.
#define LITERAL_MARK '\001'

static void set_error(char **error, const char *message)
{
    if (error != NULL && *error == NULL)
        *error = sh_strdup(message);
}

static t_token *new_token(t_token_type type, char *text, size_t start,
        int quoted)
{
    t_token *token;

    token = (t_token *)sh_calloc(1, sizeof(t_token));
    if (token == NULL) {
        free(text);
        return NULL;
    }
    token->type = type;
    token->text = text;
    token->start = start;
    token->quoted = quoted;
    return token;
}

static int push_token(t_token **head, t_token **tail, t_token *node)
{
    if (node == NULL)
        return 1;
    if (*head == NULL)
        *head = node;
    else
        (*tail)->next = node;
    *tail = node;
    return 0;
}

static int push_operator(t_token **head, t_token **tail, t_token_type type,
        const char *text, size_t start)
{
    char *copy;

    copy = sh_strdup(text);
    if (copy == NULL)
        return 1;
    return push_token(head, tail, new_token(type, copy, start, 0));
}

static int is_operator_char(char c)
{
    return (c == '|' || c == '<' || c == '>' || c == '&' || c == ';');
}

static int is_shell_space(char c)
{
    return (c == ' ' || c == '\t' || c == '\n'
        || c == '\r' || c == '\v' || c == '\f');
}

static int append_literal(t_string_builder *word, char c)
{
    return (string_builder_append_char(word, LITERAL_MARK) != 0
        || string_builder_append_char(word, c) != 0);
}

// [INTV:ARCH] 작은따옴표와 큰따옴표를 여기서부터 다르게 취급한다: 작은따옴표 안 문자는
// append_literal로 LITERAL_MARK를 붙여 "나중에도 확장 대상이 아님"을 새기고, 큰따옴표 안
// 문자는 그냥 append_char로 넣어 나중에 expand_word()가 $변수 확장을 하게 놔둔다. 즉 따옴표
// 종류에 따른 확장 여부 차이를 여기서 인코딩해두고, 실제 확장은 expand 단계로 미룬다.
static char *read_word(const char *line, size_t *i, char **error,
        int *quoted)
{
    t_string_builder    word;
    char                quote;

    if (string_builder_init(&word) != 0)
        return NULL;
    *quoted = 0;
    while (line[*i] != '\0' && !is_shell_space(line[*i])
        && !is_operator_char(line[*i])) {
        if (line[*i] == '\'' || line[*i] == '"') {
            quote = line[*i];
            *quoted = 1;
            (*i)++;
            while (line[*i] != '\0' && line[*i] != quote) {
                int failed;

                if (quote == '\'')
                    failed = append_literal(&word, line[*i]);
                else
                    failed = string_builder_append_char(&word, line[*i]);
                if (failed != 0) {
                    string_builder_discard(&word);
                    return NULL;
                }
                (*i)++;
            }
            if (line[*i] == '\0') {
                string_builder_discard(&word);
                set_error(error, "syntax error: unclosed quote");
                return NULL;
            }
            (*i)++;
        } else {
            if (string_builder_append_char(&word, line[*i]) != 0) {
                string_builder_discard(&word);
                return NULL;
            }
            (*i)++;
        }
    }
    return string_builder_take(&word);
}

static int push_word(const char *line, size_t *i, char **error,
        t_token **head, t_token **tail)
{
    size_t  start;
    char    *word;
    int     quoted;

    start = *i;
    word = read_word(line, i, error, &quoted);
    if (word == NULL)
        return 1;
    return push_token(head, tail,
        new_token(TOK_WORD, word, start, quoted));
}

t_token *tokenize_line(const char *line, char **error)
{
    t_token *head;
    t_token *tail;
    size_t  i;
    int     failed;

    head = NULL;
    tail = NULL;
    i = 0;
    failed = 0;
    if (error != NULL)
        *error = NULL;
    while (line != NULL && line[i] != '\0' && !failed) {
        while (is_shell_space(line[i]))
            i++;
        if (line[i] == '\0')
            break;
        if (line[i] == '|' && line[i + 1] == '|') {
            failed = push_operator(&head, &tail, TOK_OR, "||", i);
            i += 2;
        } else if (line[i] == '|') {
            failed = push_operator(&head, &tail, TOK_PIPE, "|", i);
            i++;
        } else if (line[i] == '&' && line[i + 1] == '&') {
            failed = push_operator(&head, &tail, TOK_AND, "&&", i);
            i += 2;
        } else if (line[i] == '&') {
            // [INTV:ARCH] 배경 실행(`cmd &`)은 이 셸의 지원 범위 밖이라, 토크나이저 단계에서
            // 바로 에러로 끊는다 — 파서까지 넘겨서 애매하게 무시되거나 잘못 해석되는 것보다,
            // 지원하지 않는 문법임을 최대한 이른 단계에서 명확히 알리는 편을 택했다.
            set_error(error, "syntax error: unsupported operator '&'");
            free_tokens(head);
            return NULL;
        } else if (line[i] == ';') {
            failed = push_operator(&head, &tail, TOK_SEQ, ";", i);
            i++;
        } else if (line[i] == '<' && line[i + 1] == '<') {
            failed = push_operator(&head, &tail, TOK_HEREDOC, "<<", i);
            i += 2;
        } else if (line[i] == '<') {
            failed = push_operator(&head, &tail, TOK_REDIR_IN, "<", i);
            i++;
        } else if (line[i] == '>' && line[i + 1] == '>') {
            failed = push_operator(&head, &tail, TOK_REDIR_APPEND, ">>", i);
            i += 2;
        } else if (line[i] == '>') {
            failed = push_operator(&head, &tail, TOK_REDIR_OUT, ">", i);
            i++;
        } else {
            failed = push_word(line, &i, error, &head, &tail);
        }
    }
    if (failed) {
        set_error(error, "allocation failure");
        free_tokens(head);
        return NULL;
    }
    return head;
}

void free_tokens(t_token *tokens)
{
    t_token *next;

    while (tokens != NULL) {
        next = tokens->next;
        free(tokens->text);
        free(tokens);
        tokens = next;
    }
}
