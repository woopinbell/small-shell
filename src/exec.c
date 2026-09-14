#define _POSIX_C_SOURCE 200809L

#include "exec_internal.h"
#include "runtime.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static void close_pipes(int (*pipes)[2], size_t pipe_count)
{
    size_t i;

    if (pipes == NULL)
        return;
    for (i = 0; i < pipe_count; i++) {
        if (pipes[i][0] >= 0)
            close(pipes[i][0]);
        if (pipes[i][1] >= 0)
            close(pipes[i][1]);
    }
}

// [INTV:EDGE] 종료 상태를 128+시그널 번호로 인코딩하는 건 셸의 관례(bash 포함)다 — 이 값
// 자체는 POSIX가 강제하지 않지만, 테스트(signal_exit_status)가 이 관례를 기대하고 있어서
// 그대로 맞췄다.
static int status_from_wait(int wait_status)
{
    if (WIFEXITED(wait_status))
        return WEXITSTATUS(wait_status);
    if (WIFSIGNALED(wait_status))
        return 128 + WTERMSIG(wait_status);
    return 1;
}

static void child_die(const char *what)
{
    fprintf(stderr, "small-shell: %s: %s\n", what, strerror(errno));
    _exit(1);
}

// [INTV:ARCH] 파이프 연결(dup2)을 먼저 하고, 그 다음에 exec_apply_redirections()로 명령에
// 명시된 리다이렉션을 적용한다 — 이 순서 덕분에 `cmd > file | cat`처럼 파이프와 명시적
// 리다이렉션이 같은 명령에 동시에 걸리면 "나중에 적용된 쪽"인 파일 리다이렉션이 파이프
// 연결을 덮어써서 이긴다(bash와 동일). 순서를 뒤집으면 명시적으로 파일을 지정했는데도
// 파이프로 나가는, 사용자 의도와 반대되는 동작이 된다.
static void run_child(t_shell *shell, const t_pipeline *pipeline, const t_command *command,
    const struct exec_context *ctx, int (*pipes)[2], size_t pipe_count, size_t index)
{
    if (index > 0 && shell_dup2(pipes[index - 1][0], STDIN_FILENO) < 0)
        child_die("dup2");
    if (index + 1 < pipeline->command_count && shell_dup2(pipes[index][1], STDOUT_FILENO) < 0)
        child_die("dup2");
    // [INTV:TRAP] dup2로 필요한 fd를 복제한 직후, 파이프의 원본 fd들은 전부 닫는다 — 자식이
    // 쓰지 않는 파이프 fd를 열어둔 채로 두면(특히 다른 세그먼트의 읽기 끝) 그 파이프의
    // 쓰기 끝을 가진 프로세스가 끝나도 이 자식이 여전히 읽기 끝을 들고 있어 EOF가 전파되지
    // 않는다 — 파이프라인 뒷단이 입력이 끝났는데도 영원히 대기하는 행 원인이 된다.
    close_pipes(pipes, pipe_count);

    if (exec_apply_redirections(command, ctx) != 0)
        _exit(1);
    if (command->argc == 0)
        _exit(0);
    if (builtin_is_known(command->argv[0])) {
        int status;

        status = builtin_run(shell, command->argv);
        fflush(stdout);
        fflush(stderr);
        _exit(status & 0xff);
    }

    {
        char **envp;

        envp = env_to_environ(shell->env);
        if (envp == NULL) {
            fprintf(stderr, "small-shell: allocation failure\n");
            _exit(1);
        }
        environ = envp;
        execvp(command->argv[0], command->argv);
        {
            int err;

            err = errno;
            fprintf(stderr, "small-shell: %s: %s\n", command->argv[0], strerror(err));
            sh_free_words(envp);
            // [INTV:EDGE] execvp 실패 시 종료 코드를 errno로 구분한다: 명령을 못 찾음
            // (ENOENT) -> 127, 찾았지만 실행 권한/형식 문제 등 -> 126. 둘 다 셸 관례로 굳어진
            // 값이라(bash 포함) 테스트가 이 구분을 그대로 검증한다.
            if (err == ENOENT)
                _exit(127);
            _exit(126);
        }
    }
}

// [INTV:TRADE_OFF] 파이프라인 중간에 fork()가 실패하면, 이미 fork된 앞쪽 자식들을 그냥
// 두지 않고 SIGKILL로 강제 종료한다 — 그 대가로 앞쪽 명령이 하던 작업이 중간에 끊기지만,
// 대신 fork 실패로 파이프라인이 완성되지 못한 상태에서 이미 뜬 자식들이 서로의 파이프
// 상대가 없어진 채로 무한정 남아있는(부모가 놓친 좀비/미아 프로세스) 상황을 막는다.
static void terminate_children(const pid_t *pids, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (pids[i] > 0 && kill(pids[i], SIGKILL) < 0 && errno != ESRCH)
            fprintf(stderr, "small-shell: kill: %s\n", strerror(errno));
    }
}

// [INTV:TRAP] EINTR(시그널에 의한 대기 중단)은 실패로 세지 않고 그냥 재시도한다 — 반면
// EINTR이 아닌 다른 waitpid 에러는 최대 2번까지만 재시도하고 포기한다. 이 둘을 같은 재시도
// 횟수로 묶으면, 진짜 에러 상황(예: ECHILD)에서도 시그널처럼 여겨 무한 재시도하게 되거나,
// 반대로 정상적인 시그널 중단까지 "에러 2회"로 세어 자식을 놓친 것처럼 오판할 수 있다.
static int wait_for_child(pid_t pid, int *wait_status)
{
    int attempts;
    int had_error;

    attempts = 0;
    had_error = 0;
    while (attempts < 2) {
        pid_t waited;

        waited = shell_waitpid(pid, wait_status, 0);
        if (waited == pid)
            return had_error;
        if (waited < 0 && errno == EINTR)
            continue;
        fprintf(stderr, "small-shell: waitpid: %s\n", strerror(errno));
        had_error = 1;
        attempts++;
    }
    return -1;
}

static int run_forked_pipeline(t_shell *shell, const t_pipeline *pipeline, const struct exec_context *ctx)
{
    size_t pipe_count;
    int (*pipes)[2];
    pid_t *pids;
    const t_command *command;
    size_t i;
    size_t spawned;
    int result;
    int wait_error;

    pipe_count = pipeline->command_count - 1;
    pipes = NULL;
    pids = NULL;
    spawned = 0;
    result = 1;
    wait_error = 0;

    if (pipe_count > 0) {
        pipes = (int (*)[2])shell_calloc(pipe_count, sizeof(int[2]));
        if (pipes == NULL)
            goto alloc_error;
        for (i = 0; i < pipe_count; i++) {
            pipes[i][0] = -1;
            pipes[i][1] = -1;
        }
    }
    pids = (pid_t *)shell_calloc(pipeline->command_count, sizeof(pid_t));
    if (pids == NULL)
        goto alloc_error;

    if (pipe_count > 0) {
        for (i = 0; i < pipe_count; i++) {
            if (shell_pipe(pipes[i]) < 0) {
                fprintf(stderr, "small-shell: pipe: %s\n", strerror(errno));
                close_pipes(pipes, pipe_count);
                free(pipes);
                free(pids);
                return 1;
            }
        }
    }

    command = pipeline->commands;
    for (i = 0; i < pipeline->command_count && command != NULL; i++) {
        pid_t pid;

        pid = shell_fork();
        if (pid < 0) {
            fprintf(stderr, "small-shell: fork: %s\n", strerror(errno));
            break;
        }
        if (pid == 0)
            run_child(shell, pipeline, command, ctx, pipes, pipe_count, i);
        pids[i] = pid;
        spawned++;
        command = command->next;
    }

    // [INTV:TRAP] 부모도 모든 파이프 fd를 자식들에게 dup2로 넘긴 직후 즉시 닫는다 — 부모가
    // 파이프 읽기/쓰기 끝을 계속 들고 있으면, 마지막 자식이 끝나도 부모가 쓰기 끝을 쥔 채
    // 남아 있어 "입력 끝(EOF)"이 절대 도달하지 않는 것과 같은 문제가 생긴다.
    close_pipes(pipes, pipe_count);
    if (spawned != pipeline->command_count)
        terminate_children(pids, spawned);
    for (i = 0; i < spawned; i++) {
        int wait_status;
        int wait_result;

        wait_result = wait_for_child(pids[i], &wait_status);
        // [INTV:ARCH] 파이프라인 전체의 종료 상태는 마지막 명령의 상태로 정한다(POSIX 관례) —
        // 그래서 spawned 중 마지막 인덱스(i + 1 == command_count)의 wait 결과만 result에 반영.
        if (wait_result == 0 && i + 1 == pipeline->command_count)
            result = status_from_wait(wait_status);
        if (wait_result != 0)
            wait_error = 1;
    }

    if (wait_error)
        result = 1;
#ifdef SMALL_SHELL_TESTING
    if (getenv("SMALL_SHELL_CHECK_CHILDREN") != NULL
        && !shell_children_reaped()) {
        fprintf(stderr, "small-shell: unreaped child process\n");
        result = 1;
    }
#endif
    free(pids);
    free(pipes);
    return spawned == pipeline->command_count ? result : 1;

alloc_error:
    fprintf(stderr, "small-shell: allocation failure\n");
    close_pipes(pipes, pipe_count);
    free(pipes);
    free(pids);
    return 1;
}

static int expand_one_pipeline(t_shell *shell, t_pipeline *pipeline)
{
    t_pipeline *next;
    int result;

    // [INTV:TRAP] expand_pipeline()은 연결 리스트를 next 포인터를 따라 끝까지 훑는 함수라,
    // 이 pipeline 하나만 확장하려고 호출하려면 next를 일시적으로 끊어야 한다 — 안 끊으면
    // 뒤에 이어진 pipeline들까지 여기서 먼저 확장돼버려서, `&&`/`||` 게이트에 걸려 실행되지
    // 않을 수도 있는 pipeline의 부수효과(예: 잘못된 확장으로 인한 에러 출력)가 실행 순서보다
    // 먼저 나타난다.
    next = pipeline->next;
    pipeline->next = NULL;
    shell_runtime_set_alloc_scope("expand");
    result = expand_pipeline(shell, pipeline);
    pipeline->next = next;
    return result;
}

// [INTV:ARCH] 파이프라인이 명령 1개뿐이고 그 명령이 parent-전용 빌트인(cd/export/unset/exit
// 등)이면 fork하지 않고 부모 프로세스 안에서 바로 실행한다.
// - [TRAP] cd나 export를 자식 프로세스 안에서 실행하면 그 자식이 종료되는 순간 변경 사항
//   (작업 디렉터리, 환경변수)도 함께 사라진다 — 셸 자신의 상태를 바꾸는 빌트인은 반드시
//   부모(셸 프로세스) 안에서 실행해야 의미가 있다. 파이프라인의 일부로 쓰인 cd(`cd a | cat`)
//   처럼 여러 명령 중 하나일 때는 부모 상태 변경 자체가 애매해지므로 이 최적화 경로를
//   타지 않고 forked 경로로 보낸다.
static int execute_one_pipeline(t_shell *shell, t_pipeline *pipeline, const struct exec_context *ctx)
{
    const t_command *command;

    if (pipeline == NULL || pipeline->command_count == 0)
        return shell->last_status;
    if (expand_one_pipeline(shell, pipeline) != 0) {
        fprintf(stderr, "small-shell: allocation failure\n");
        return 1;
    }
    shell_runtime_set_alloc_scope("execute");
    command = pipeline->commands;
    if (pipeline->command_count == 1
        && (command->argc == 0 || builtin_is_parent(command->argv[0])))
        return exec_run_parent_command(shell, command, ctx);
    return run_forked_pipeline(shell, pipeline, ctx);
}

static int execute_pipeline_list_ctx(t_shell *shell, t_pipeline *pipeline, const struct exec_context *ctx)
{
    t_connector previous;

    previous = CONN_NONE;
    while (pipeline != NULL && shell->running) {
        int should_run;

        should_run = 1;
        if (previous == CONN_AND && shell->last_status != 0)
            should_run = 0;
        else if (previous == CONN_OR && shell->last_status == 0)
            should_run = 0;
        if (should_run)
            shell->last_status = execute_one_pipeline(shell, pipeline, ctx);
        previous = pipeline->next_op;
        pipeline = pipeline->next;
    }
    return shell->last_status;
}

int execute_pipeline_list(t_shell *shell, t_pipeline *pipeline)
{
    struct exec_context ctx;

    ctx.shell = shell;
    ctx.heredocs = NULL;
    shell_runtime_set_alloc_scope("heredoc");
    return execute_pipeline_list_ctx(shell, pipeline, &ctx);
}

// [INTV:TRADE_OFF] 에러 문자열을 다시 파싱해서 "allocation failure"인지 아닌지로 상태 코드를
// 나누는 대신(예: 에러를 enum으로 관리), 문자열 비교로 처리했다 — parse_tokens/tokenize_line이
// 이미 사람이 읽는 에러 메시지를 char* error로 돌려주는 구조라, 별도의 에러 코드 enum을
// 추가하는 것보다 기존 문자열 채널을 재사용하는 쪽이 변경 범위가 작았다. 대가는 에러 메시지
// 문구("allocation failure")가 바뀌면 이 분기도 같이 깨진다는 점.
static int process_error_status(const char *error)
{
    if (error != NULL && strcmp(error, "allocation failure") == 0)
        return 1;
    // [INTV:EDGE] 문법 오류는 일부러 정상 종료 코드 범위(0~255) 밖인 258을 쓴다 — 258 & 0xff
    // == 2로, 프로세스가 실제로 종료될 때(main.c의 normalize_status)는 bash가 문법 오류에
    // 쓰는 관례적 값 2로 자연히 맞춰지면서도, 같은 세션 안에서 `$?`로 조회할 때는(마스킹
    // 전이라) 258 그대로 보여 "이게 명령 실행 실패가 아니라 파싱 단계 실패였다"를 구분할 수
    // 있다 — tests/smoke.sh의 syntax_error_status 케이스가 이 258을 그대로 기대한다.
    return 258;
}

int shell_process_line(t_shell *shell, const char *line)
{
    t_token *tokens;
    t_pipeline *pipelines;
    struct exec_context ctx;
    char *error;

    if (shell == NULL || line == NULL || line[0] == '\0')
        return shell != NULL ? shell->last_status : 1;

    shell_runtime_begin_command();

    error = NULL;
    shell_runtime_set_alloc_scope("token");
    errno = 0;
    tokens = tokenize_line(line, &error);
    if (error != NULL || (tokens == NULL && errno == ENOMEM)) {
        shell->last_status = error != NULL
            ? process_error_status(error) : 1;
        fprintf(stderr, "small-shell: %s\n",
            error != NULL ? error : "allocation failure");
        free(error);
        return shell->last_status;
    }

    shell_runtime_set_alloc_scope("parser");
    errno = 0;
    pipelines = parse_tokens(tokens, &error);
    free_tokens(tokens);
    if (error != NULL || (pipelines == NULL && errno == ENOMEM)) {
        shell->last_status = error != NULL
            ? process_error_status(error) : 1;
        fprintf(stderr, "small-shell: %s\n",
            error != NULL ? error : "allocation failure");
        free(error);
        return shell->last_status;
    }
    if (pipelines == NULL)
        return shell->last_status;

    ctx.shell = shell;
    ctx.heredocs = NULL;
    shell_runtime_set_alloc_scope("heredoc");
    if (exec_prepare_heredocs(&ctx, pipelines) != 0) {
        exec_heredoc_entries_free(ctx.heredocs);
        free_pipeline(pipelines);
        shell->last_status = 1;
        return shell->last_status;
    }

    (void)execute_pipeline_list_ctx(shell, pipelines, &ctx);
    exec_heredoc_entries_free(ctx.heredocs);
    free_pipeline(pipelines);
    return shell->last_status;
}
