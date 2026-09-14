#define _POSIX_C_SOURCE 200809L

#include "shell.h"
#include "runtime.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int builtin_is_known(const char *name)
{
    static const char *builtins[] = {
        "echo", "pwd", "cd", "env", "export", "unset", "exit", NULL
    };
    size_t i;

    if (name == NULL)
        return 0;
    for (i = 0; builtins[i] != NULL; i++) {
        if (strcmp(name, builtins[i]) == 0)
            return 1;
    }
    return 0;
}

// [INTV:ARCH] 이 셸에서는 "부모 프로세스 안에서 실행해야 하는 빌트인"과 "알려진 빌트인
// 이름"이 사실상 같은 집합이라 builtin_is_parent()가 그냥 builtin_is_known()을 그대로
// 위임한다 — 함수를 따로 나눈 이유는, 만약 나중에 (예를 들어 파이프라인의 일부가 아닐 때만
// 부모에서 도는 빌트인처럼) 두 집합이 갈라지는 요구가 생기면 호출부(exec.c) 코드는 그대로
// 두고 이 함수 안의 판단만 바꾸면 되게 하기 위함이다.
int builtin_is_parent(const char *name)
{
    return builtin_is_known(name);
}

// [INTV:EDGE] `-n` 옵션은 문자가 전부 'n'으로만 이루어진 경우에만(`-n`, `-nn`, ...) 인식하고,
// `-ne`처럼 다른 문자가 섞이면 옵션이 아니라 첫 인자로 취급해 멈춘다 — bash의 echo 빌트인이
// 옵션 파싱을 느슨하게 하는 방식을 그대로 따른 것.
static int builtin_echo(char **argv)
{
    size_t  i;
    int     newline;

    newline = 1;
    i = 1;
    while (argv[i] != NULL && argv[i][0] == '-' && argv[i][1] == 'n') {
        size_t  j;
        int     only_n;

        only_n = 1;
        for (j = 1; argv[i][j] != '\0'; j++) {
            if (argv[i][j] != 'n') {
                only_n = 0;
                break;
            }
        }
        if (!only_n)
            break;
        newline = 0;
        i++;
    }
    while (argv[i] != NULL) {
        if (shell_write_text(STDOUT_FILENO, argv[i]) != 0)
            return 1;
        if (argv[i + 1] != NULL
            && shell_write_text(STDOUT_FILENO, " ") != 0)
            return 1;
        i++;
    }
    if (newline && shell_write_text(STDOUT_FILENO, "\n") != 0)
        return 1;
    return 0;
}

static int builtin_pwd(void)
{
    char *cwd;

    // [INTV:EDGE] getcwd(NULL, 0)는 POSIX.1-2008/glibc 확장으로, 버퍼 크기를 미리 계산할
    // 필요 없이 필요한 만큼 malloc해서 돌려준다 — 고정 크기 버퍼(PATH_MAX 등)를 썼다면
    // 그보다 긴 경로에서 잘리거나 실패할 수 있었다.
    cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        fprintf(stderr, "small-shell: pwd: %s\n", strerror(errno));
        return 1;
    }
    if (shell_write_text(STDOUT_FILENO, cwd) != 0
        || shell_write_text(STDOUT_FILENO, "\n") != 0) {
        fprintf(stderr, "small-shell: pwd: %s\n", strerror(errno));
        free(cwd);
        return 1;
    }
    free(cwd);
    return 0;
}

static size_t argv_count(char **argv)
{
    size_t count;

    count = 0;
    while (argv != NULL && argv[count] != NULL)
        count++;
    return count;
}

// [INTV:ARCH] - [FLOW] cd 인자 해석 순서: 1. 인자 없음 -> HOME 필요 -> 2. `-`(하이픈)
// -> OLDPWD로 이동하고 이동한 경로를 출력(print_target) -> 3. 그 외 -> 인자를 경로로 그대로
// 사용. bash의 `cd -` 관례(직전 디렉터리로 복귀 + 그 경로 출력)를 그대로 재현한다.
static int builtin_cd(t_shell *shell, char **argv)
{
    const char  *target;
    char        *old_pwd;
    char        *new_pwd;
    int         print_target;
    int         status;

    if (argv_count(argv) > 2) {
        fprintf(stderr, "small-shell: cd: too many arguments\n");
        return 1;
    }
    print_target = 0;
    if (argv[1] == NULL) {
        target = env_get(shell->env, "HOME");
        if (target == NULL || target[0] == '\0') {
            fprintf(stderr, "small-shell: cd: HOME not set\n");
            return 1;
        }
    } else if (strcmp(argv[1], "-") == 0) {
        target = env_get(shell->env, "OLDPWD");
        if (target == NULL || target[0] == '\0') {
            fprintf(stderr, "small-shell: cd: OLDPWD not set\n");
            return 1;
        }
        print_target = 1;
    } else {
        target = argv[1];
    }
    old_pwd = getcwd(NULL, 0);
    if (chdir(target) != 0) {
        fprintf(stderr, "small-shell: cd: %s: %s\n", target, strerror(errno));
        free(old_pwd);
        return 1;
    }
    new_pwd = getcwd(NULL, 0);
    status = 0;
    // [INTV:ARCH] cd는 OLDPWD/PWD 두 환경변수를 갱신해야 다음 `cd -`가 동작하고, 서브셸을
    // fork할 때 자식이 올바른 PWD를 물려받는다 — chdir(2) 자체는 프로세스의 작업 디렉터리만
    // 바꿀 뿐 이 두 변수를 자동으로 갱신해주지 않으므로 셸이 직접 관리해야 한다.
    if (old_pwd != NULL && env_set(&shell->env, "OLDPWD", old_pwd, 1) != 0)
        status = 1;
    if (new_pwd != NULL && env_set(&shell->env, "PWD", new_pwd, 1) != 0)
        status = 1;
    if (print_target && new_pwd != NULL
        && (shell_write_text(STDOUT_FILENO, new_pwd) != 0
            || shell_write_text(STDOUT_FILENO, "\n") != 0))
        status = 1;
    free(old_pwd);
    free(new_pwd);
    return status;
}

static int builtin_env(t_shell *shell, char **argv)
{
    if (argv[1] != NULL) {
        fprintf(stderr, "small-shell: env: arguments are not supported\n");
        return 1;
    }
    return env_print(shell->env, 0);
}

static int split_assignment(const char *arg, char **key, const char **value)
{
    size_t len;

    len = 0;
    while (arg[len] != '\0' && arg[len] != '=')
        len++;
    *key = shell_strndup(arg, len);
    if (*key == NULL)
        return 1;
    *value = arg[len] == '=' ? arg + len + 1 : NULL;
    return 0;
}

static int valid_assignment_name(const char *key)
{
    size_t i;

    if (!sh_is_name_start((unsigned char)key[0]))
        return 0;
    i = 1;
    while (key[i] != '\0') {
        if (!sh_is_name_char((unsigned char)key[i]))
            return 0;
        i++;
    }
    return 1;
}

// [INTV:EDGE] `export FOO`(값 없는 대입)와 `export FOO=bar`를 split_assignment가 구분한다
// — '='가 아예 없으면 value는 NULL이고, env_set()에서 NULL은 "값은 바꾸지 말고 exported
// 플래그만 켜라"는 의미로 처리된다(env.c 참고). 즉 이미 값이 있는 변수를 export만 시키는
// 경우와, 새 변수를 빈 값으로 export하는 경우가 여기서 갈린다.
static int builtin_export(t_shell *shell, char **argv)
{
    size_t  i;
    int     status;

    if (argv[1] == NULL)
        return env_print(shell->env, 1);
    status = 0;
    for (i = 1; argv[i] != NULL; i++) {
        char        *key;
        const char  *value;

        key = NULL;
        value = NULL;
        if (split_assignment(argv[i], &key, &value) != 0) {
            fprintf(stderr, "small-shell: export: allocation failure\n");
            return 1;
        }
        if (!valid_assignment_name(key)) {
            fprintf(stderr,
                "small-shell: export: `%s': not a valid identifier\n",
                argv[i]);
            free(key);
            status = 1;
            continue;
        }
        if (env_set(&shell->env, key, value, 1) != 0) {
            fprintf(stderr, "small-shell: export: allocation failure\n");
            free(key);
            return 1;
        }
        free(key);
    }
    return status;
}

static int builtin_unset(t_shell *shell, char **argv)
{
    size_t i;

    for (i = 1; argv[i] != NULL; i++)
        (void)env_unset(&shell->env, argv[i]);
    return 0;
}

static int parse_exit_status(const char *s, int *status)
{
    char    *end;
    long    value;

    errno = 0;
    value = strtol(s, &end, 10);
    if (s == end || *end != '\0' || errno == ERANGE)
        return 0;
    // [INTV:EDGE] exit 코드는 unsigned char로 잘라 저장한다 — `exit 256`이 셸 관례상 `exit 0`과
    // 같은 값(256 & 0xff == 0)이 되는 bash 동작을 그대로 재현한다.
    *status = (unsigned char)value;
    return 1;
}

static int builtin_exit(t_shell *shell, char **argv)
{
    int status;

    if (argv[1] == NULL) {
        shell->running = 0;
        return shell->last_status;
    }
    if (!parse_exit_status(argv[1], &status)) {
        fprintf(stderr, "small-shell: exit: %s: numeric argument required\n",
            argv[1]);
        shell->last_status = 2;
        shell->running = 0;
        return 2;
    }
    if (argv[2] != NULL) {
        fprintf(stderr, "small-shell: exit: too many arguments\n");
        return 1;
    }
    shell->last_status = status;
    shell->running = 0;
    return status;
}

int builtin_run(t_shell *shell, char **argv)
{
    if (shell == NULL || argv == NULL || argv[0] == NULL)
        return 0;
    if (strcmp(argv[0], "echo") == 0)
        return builtin_echo(argv);
    if (strcmp(argv[0], "pwd") == 0)
        return builtin_pwd();
    if (strcmp(argv[0], "cd") == 0)
        return builtin_cd(shell, argv);
    if (strcmp(argv[0], "env") == 0)
        return builtin_env(shell, argv);
    if (strcmp(argv[0], "export") == 0)
        return builtin_export(shell, argv);
    if (strcmp(argv[0], "unset") == 0)
        return builtin_unset(shell, argv);
    if (strcmp(argv[0], "exit") == 0)
        return builtin_exit(shell, argv);
    return 127;
}
