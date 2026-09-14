#include "shell.h"

#include <stdlib.h>

static void free_commands(t_command *cmd);

static void set_error(char **error, const char *message)
{
    if (error != NULL && *error == NULL)
        *error = sh_strdup(message);
}

static t_command *new_command(void)
{
    return (t_command *)sh_calloc(1, sizeof(t_command));
}

static t_pipeline *new_pipeline(void)
{
    t_pipeline *pipeline;

    pipeline = (t_pipeline *)sh_calloc(1, sizeof(t_pipeline));
    if (pipeline != NULL)
        pipeline->next_op = CONN_NONE;
    return pipeline;
}

static size_t word_count(char **argv)
{
    size_t n;

    n = 0;
    while (argv != NULL && argv[n] != NULL)
        n++;
    return n;
}

// [INTV:TRADE_OFF] 인자를 추가할 때마다 argv 배열 전체를 새로 calloc해서 복사한다 —
// 명령당 인자 수가 O(n)일 때 전체가 O(n^2)이 되는 대가를 감수하고, 대신 t_command에
// "용량(capacity)"을 따로 추적하는 필드/성장 로직이 없어 구조체가 단순해진다. 셸 명령 한
// 줄의 인자 개수는 사실상 작기 때문에 이 비효율이 체감되지 않는다.
static int add_arg(t_command *cmd, const char *text)
{
    size_t  n;
    char    **next;
    char    *copy;
    size_t  i;

    n = word_count(cmd->argv);
    next = (char **)sh_calloc(n + 2, sizeof(char *));
    if (next == NULL)
        return 1;
    copy = sh_strdup(text);
    if (copy == NULL) {
        free(next);
        return 1;
    }
    i = 0;
    while (i < n) {
        next[i] = cmd->argv[i];
        i++;
    }
    next[n] = copy;
    free(cmd->argv);
    cmd->argv = next;
    cmd->argc = n + 1;
    return 0;
}

static int add_redir(t_command *cmd, t_redir_type type, const char *target,
        int target_quoted)
{
    t_redir *node;
    t_redir *tail;

    node = (t_redir *)sh_calloc(1, sizeof(t_redir));
    if (node == NULL)
        return 1;
    node->target = sh_strdup(target);
    if (node->target == NULL) {
        free(node);
        return 1;
    }
    node->type = type;
    node->heredoc_quoted = (type == REDIR_HEREDOC && target_quoted);
    // [INTV:ARCH] - [TRAP] 리다이렉션은 리스트 맨 끝에 이어붙인다(맨 앞이 아니라). 뒤에서
    // exec_apply_redirections()가 이 리스트를 순서대로 처리하며 각 fd를 계속 갈아치우므로
    // `> a > b`처럼 같은 방향 리다이렉트가 여러 번 나오면 "마지막에 적용된 것이 이긴다"는
    // bash 동작이 성립한다 — 순서를 뒤집으면 정반대 결과가 나온다.
    if (cmd->redirs == NULL) {
        cmd->redirs = node;
        return 0;
    }
    tail = cmd->redirs;
    while (tail->next != NULL)
        tail = tail->next;
    tail->next = node;
    return 0;
}

static int command_empty(t_command *cmd)
{
    return (cmd == NULL || (cmd->argc == 0 && cmd->redirs == NULL));
}

static void append_command(t_pipeline *pipeline, t_command *cmd)
{
    t_command *tail;

    if (pipeline->commands == NULL) {
        pipeline->commands = cmd;
        pipeline->command_count++;
        return;
    }
    tail = pipeline->commands;
    while (tail->next != NULL)
        tail = tail->next;
    tail->next = cmd;
    pipeline->command_count++;
}

static void append_pipeline(t_pipeline **head, t_pipeline **tail,
        t_pipeline *node)
{
    if (*head == NULL)
        *head = node;
    else
        (*tail)->next = node;
    *tail = node;
}

static int token_is_redir(t_token_type type)
{
    return (type == TOK_REDIR_IN || type == TOK_REDIR_OUT
        || type == TOK_REDIR_APPEND || type == TOK_HEREDOC);
}

static t_redir_type redir_type(t_token_type type)
{
    if (type == TOK_REDIR_OUT)
        return REDIR_OUT;
    if (type == TOK_REDIR_APPEND)
        return REDIR_APPEND;
    if (type == TOK_HEREDOC)
        return REDIR_HEREDOC;
    return REDIR_IN;
}

static t_connector connector_type(t_token_type type)
{
    if (type == TOK_AND)
        return CONN_AND;
    if (type == TOK_OR)
        return CONN_OR;
    return CONN_SEQ;
}

// [INTV:ARCH] 파싱 도중 어디서 실패하든 지금까지 만든 cmd/pipeline/head를 전부 한 곳에서
// 해제하고 NULL을 반환하는 공용 실패 경로. 이게 없으면 parse_tokens() 안의 실패 분기마다
// 세 가지 자원을 각각 해제하는 코드를 반복해야 해서 하나라도 빠뜨리기 쉽다.
static t_pipeline *parse_failure(t_pipeline *head, t_pipeline *pipeline,
        t_command *cmd, char **error, const char *message)
{
    set_error(error, message);
    free_commands(cmd);
    free_pipeline(pipeline);
    free_pipeline(head);
    return NULL;
}

// [INTV:ARCH] - [FLOW] 토큰 스트림을 단일 패스로 훑으며 세 단계 구조(sequence -> pipeline
// -> command)를 동시에 조립한다: 1. TOK_WORD는 현재 cmd에 인자로 누적 -> 2. 리다이렉션
// 토큰은 다음 토큰을 타깃으로 삼아 cmd에 붙임 -> 3. '|'는 현재 cmd를 pipeline에 매듭짓고
// 새 cmd를 염 -> 4. ';'/'&&'/'||'는 현재 pipeline을 sequence에 매듭짓고 새 pipeline을 염.
// 별도의 재귀 하강 파서 대신 순차 상태 전이로 처리한 이유는 이 문법이 중첩이 없는
// (괄호·서브셸 없음) 평평한 구조라 상태 머신만으로 충분하기 때문이다.
t_pipeline *parse_tokens(t_token *tokens, char **error)
{
    t_pipeline  *head;
    t_pipeline  *tail;
    t_pipeline  *pipeline;
    t_command   *cmd;
    t_token     *cur;
    t_token_type last_connector;
    int         after_pipe;

    head = NULL;
    tail = NULL;
    pipeline = new_pipeline();
    cmd = new_command();
    cur = tokens;
    last_connector = TOK_WORD;
    after_pipe = 0;
    if (error != NULL)
        *error = NULL;
    if (pipeline == NULL || cmd == NULL)
        return parse_failure(head, pipeline, cmd, error,
            "allocation failure");
    while (cur != NULL) {
        if (cur->type == TOK_WORD) {
            if (add_arg(cmd, cur->text) != 0)
                return parse_failure(head, pipeline, cmd, error,
                    "allocation failure");
            after_pipe = 0;
        } else if (token_is_redir(cur->type)) {
            if (cur->next == NULL || cur->next->type != TOK_WORD)
                return parse_failure(head, pipeline, cmd, error,
                    "syntax error: redirection target missing");
            if (add_redir(cmd, redir_type(cur->type), cur->next->text,
                    cur->next->quoted) != 0)
                return parse_failure(head, pipeline, cmd, error,
                    "allocation failure");
            cur = cur->next;
            after_pipe = 0;
        } else if (cur->type == TOK_PIPE) {
            if (command_empty(cmd))
                return parse_failure(head, pipeline, cmd, error,
                    "syntax error: empty command before pipe");
            append_command(pipeline, cmd);
            cmd = new_command();
            if (cmd == NULL)
                return parse_failure(head, pipeline, cmd, error,
                    "allocation failure");
            after_pipe = 1;
        } else {
            if (after_pipe)
                return parse_failure(head, pipeline, cmd, error,
                    "syntax error: expected command after pipe");
            if (command_empty(cmd) && pipeline->commands == NULL)
                return parse_failure(head, pipeline, cmd, error,
                    "syntax error: empty command before connector");
            if (!command_empty(cmd))
                append_command(pipeline, cmd);
            else
                free_commands(cmd);
            cmd = NULL;
            pipeline->next_op = connector_type(cur->type);
            append_pipeline(&head, &tail, pipeline);
            last_connector = cur->type;
            pipeline = new_pipeline();
            cmd = new_command();
            if (pipeline == NULL || cmd == NULL)
                return parse_failure(head, pipeline, cmd, error,
                    "allocation failure");
            after_pipe = 0;
        }
        cur = cur->next;
    }
    if (after_pipe)
        return parse_failure(head, pipeline, cmd, error,
            "syntax error: expected command after pipe");
    if (!command_empty(cmd))
        append_command(pipeline, cmd);
    else
        free_commands(cmd);
    cmd = NULL;
    // [INTV:EDGE] 마지막 pipeline이 비어 있는 채로 루프를 빠져나오는 경우가 두 가지다:
    // "echo 1 &&"처럼 조건 연산자 뒤에 아무것도 없으면 문법 오류지만, "echo 1;"처럼 트레일링
    // 세미콜론 뒤에 아무것도 없는 건 bash에서도 허용되는 정상 입력이다. last_connector로 둘을
    // 구분해서 후자일 때만 빈 pipeline을 조용히 버린다.
    if (pipeline->commands == NULL) {
        free(pipeline);
        if (last_connector == TOK_AND || last_connector == TOK_OR) {
            set_error(error,
                "syntax error: conditional operator needs a following pipeline");
            free_pipeline(head);
            return NULL;
        }
        if (last_connector == TOK_SEQ && tail != NULL)
            tail->next_op = CONN_NONE;
        return head;
    }
    append_pipeline(&head, &tail, pipeline);
    return head;
}

static void free_redirs(t_redir *redir)
{
    t_redir *next;

    while (redir != NULL) {
        next = redir->next;
        free(redir->target);
        free(redir);
        redir = next;
    }
}

static void free_commands(t_command *cmd)
{
    t_command *next;

    while (cmd != NULL) {
        next = cmd->next;
        sh_free_words(cmd->argv);
        free_redirs(cmd->redirs);
        free(cmd);
        cmd = next;
    }
}

void free_pipeline(t_pipeline *pipeline)
{
    t_pipeline *next;

    while (pipeline != NULL) {
        next = pipeline->next;
        free_commands(pipeline->commands);
        free(pipeline);
        pipeline = next;
    }
}

static size_t count_pipelines(t_pipeline *pipeline)
{
    size_t count;

    count = 0;
    while (pipeline != NULL) {
        count++;
        pipeline = pipeline->next;
    }
    return count;
}

void shell_sequence_init(t_sequence *sequence)
{
    if (sequence == NULL)
        return;
    sequence->pipelines = NULL;
    sequence->pipeline_count = 0;
}

void shell_sequence_free(t_sequence *sequence)
{
    if (sequence == NULL)
        return;
    free_pipeline(sequence->pipelines);
    sequence->pipelines = NULL;
    sequence->pipeline_count = 0;
}

int shell_parse_line(const char *line, t_sequence *sequence, char **error)
{
    t_token  *tokens;
    char     *internal_error;
    char     **error_slot;

    internal_error = NULL;
    error_slot = error != NULL ? error : &internal_error;
    if (sequence == NULL) {
        set_error(error_slot, "parse output is null");
        free(internal_error);
        return 1;
    }
    shell_sequence_init(sequence);
    tokens = tokenize_line(line, error_slot);
    if (*error_slot != NULL) {
        free(internal_error);
        return 1;
    }
    sequence->pipelines = parse_tokens(tokens, error_slot);
    free_tokens(tokens);
    if (*error_slot != NULL) {
        shell_sequence_free(sequence);
        free(internal_error);
        return 1;
    }
    sequence->pipeline_count = count_pipelines(sequence->pipelines);
    free(internal_error);
    return 0;
}

// [INTV:ARCH] '&&'/'||'/';' 게이트를 pipeline 하나 실행할 때마다 검사하는 짧은 순회 루프로
// 처리한다 — 조건 연산자를 트리로 만들지 않고, "직전 pipeline의 next_op가 뭐였는지 + 직전
// 종료 status"만으로 다음 pipeline을 건너뛸지 결정하는 평평한 게이트 방식을 택했다.
// - [FLOW] 1. gate가 CONN_AND인데 직전 status != 0이면 스킵 -> 2. gate가 CONN_OR인데
//   직전 status == 0이면 스킵 -> 3. 그 외에는 실행하고 status 갱신 -> 4. 이 pipeline의
//   next_op를 다음 반복의 gate로 넘김.
int shell_execute_sequence(const t_sequence *sequence, t_env *env,
        int *last_status, const t_executor_hooks *hooks, void *ctx)
{
    const t_pipeline *pipeline;
    t_connector      gate;
    int              status;
    int              should_run;

    if (hooks == NULL || hooks->run_pipeline == NULL) {
        if (hooks != NULL && hooks->on_error != NULL)
            hooks->on_error("missing executor pipeline hook", ctx);
        if (last_status != NULL)
            *last_status = 1;
        return 1;
    }
    status = last_status != NULL ? *last_status : 0;
    gate = CONN_NONE;
    pipeline = sequence != NULL ? sequence->pipelines : NULL;
    while (pipeline != NULL) {
        should_run = 1;
        if (gate == CONN_AND && status != 0)
            should_run = 0;
        if (gate == CONN_OR && status == 0)
            should_run = 0;
        if (should_run)
            status = hooks->run_pipeline(pipeline, env, ctx);
        gate = pipeline->next_op;
        pipeline = pipeline->next;
    }
    if (last_status != NULL)
        *last_status = status;
    return status;
}
