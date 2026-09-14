#include "shell.h"
#include "runtime.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static t_env *env_new(const char *key, const char *value, int exported)
{
    t_env *node;

    node = (t_env *)sh_calloc(1, sizeof(t_env));
    if (node == NULL)
        return NULL;
    node->key = sh_strdup(key);
    node->value = sh_strdup(value != NULL ? value : "");
    if (node->key == NULL || node->value == NULL) {
        free(node->key);
        free(node->value);
        free(node);
        return NULL;
    }
    node->exported = exported ? 1 : 0;
    return node;
}

static t_env *env_find(t_env *env, const char *key)
{
    while (env != NULL) {
        if (env->key != NULL && key != NULL
            && strcmp(env->key, key) == 0)
            return env;
        env = env->next;
    }
    return NULL;
}

int shell_env_is_valid_name(const char *key)
{
    size_t i;

    if (key == NULL || !sh_is_name_start((unsigned char)key[0]))
        return 0;
    i = 1;
    while (key[i] != '\0') {
        if (!sh_is_name_char((unsigned char)key[i]))
            return 0;
        i++;
    }
    return 1;
}

// [INTV:ARCH] envp의 각 항목을 그대로 저장하지 않고, exported=1로 표시해서 t_env 리스트에
// 옮겨 담는다 — 프로세스가 물려받은 환경변수는 전부 "이미 export된 상태"로 취급해야, 이후
// env_to_environ()이 자식에게 환경을 다시 넘겨줄 때 원래 있던 변수들이 자연스럽게 포함된다.
// - [TRAP] '='가 없는 envp 항목(비정상적이지만 이론상 가능)은 조용히 건너뛴다 — key를 뽑을
//   기준이 없는 항목을 강제로 파싱하면 오히려 잘못된 변수를 만들어내게 된다.
t_env *env_from_environ(char **envp)
{
    t_env   *head;
    t_env   *tail;
    size_t  i;

    head = NULL;
    tail = NULL;
    i = 0;
    while (envp != NULL && envp[i] != NULL) {
        char    *eq;
        char    *key;
        t_env   *node;

        eq = strchr(envp[i], '=');
        if (eq != NULL) {
            key = sh_substr(envp[i], 0, (size_t)(eq - envp[i]));
            if (key == NULL) {
                env_free(head);
                return NULL;
            }
            node = env_new(key, eq + 1, 1);
            free(key);
            if (node == NULL) {
                env_free(head);
                return NULL;
            }
            if (head == NULL)
                head = node;
            else
                tail->next = node;
            tail = node;
        }
        i++;
    }
    return head;
}

void env_free(t_env *env)
{
    t_env *next;

    while (env != NULL) {
        next = env->next;
        free(env->key);
        free(env->value);
        free(env);
        env = next;
    }
}

// [INTV:EDGE] 존재하지 않는 키에 대해 NULL이 아니라 ""(빈 문자열)를 반환한다 — 호출부
// (expand.c 등)가 매번 NULL 체크를 하지 않고 바로 문자열 함수에 넘길 수 있게 하기 위함이며,
// bash가 미설정 변수를 빈 문자열로 취급하는 것과도 일치한다.
const char *env_get(t_env *env, const char *key)
{
    t_env *node;

    node = env_find(env, key);
    if (node == NULL)
        return "";
    return node->value;
}

// [INTV:EDGE] value가 NULL이면 값은 건드리지 않고 exported 플래그만 세운다 — `export EXISTING`
// (이미 값이 있는 변수를 export만 하는 경우)이 그 값을 빈 문자열로 지워버리지 않게 하는
// 분기다. 새 변수를 만들 때는 env_new()에서 NULL을 ""로 정규화하므로 이 구분이 필요 없다.
int env_set(t_env **env, const char *key, const char *value, int exported)
{
    t_env *node;
    t_env *tail;

    if (env == NULL || !shell_env_is_valid_name(key))
        return 1;
    node = env_find(*env, key);
    if (node != NULL) {
        if (value != NULL) {
            char *copy;

            copy = sh_strdup(value);
            if (copy == NULL)
                return 1;
            free(node->value);
            node->value = copy;
        }
        if (exported)
            node->exported = 1;
        return 0;
    }
    node = env_new(key, value != NULL ? value : "", exported);
    if (node == NULL)
        return 1;
    if (*env == NULL) {
        *env = node;
        return 0;
    }
    tail = *env;
    while (tail->next != NULL)
        tail = tail->next;
    tail->next = node;
    return 0;
}

int env_unset(t_env **env, const char *key)
{
    t_env *cur;
    t_env *prev;

    if (env == NULL || key == NULL)
        return 1;
    cur = *env;
    prev = NULL;
    while (cur != NULL) {
        if (cur->key != NULL && strcmp(cur->key, key) == 0) {
            if (prev != NULL)
                prev->next = cur->next;
            else
                *env = cur->next;
            cur->next = NULL;
            env_free(cur);
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}

// [INTV:ARCH] exported == 0인 변수(export되지 않은 셸 로컬 변수)는 자식에게 물려줄 environ
// 배열에서 제외한다 — export되지 않은 변수는 이 셸 프로세스 안에서만 보이는 것이 셸의
// 기본 규칙이라, execvp로 넘어갈 자식 프로세스의 환경에는 포함되면 안 된다.
char **env_to_environ(t_env *env)
{
    size_t  count;
    size_t  i;
    char    **out;
    t_env   *cur;

    count = 0;
    cur = env;
    while (cur != NULL) {
        if (cur->key != NULL && cur->exported)
            count++;
        cur = cur->next;
    }
    out = (char **)sh_calloc(count + 1, sizeof(char *));
    if (out == NULL)
        return NULL;
    i = 0;
    cur = env;
    while (cur != NULL) {
        if (cur->key != NULL && cur->exported) {
            char *pair;

            pair = sh_strjoin_free(sh_strdup(cur->key), "=");
            pair = sh_strjoin_free(pair, cur->value);
            if (pair == NULL) {
                sh_free_words(out);
                return NULL;
            }
            out[i++] = pair;
        }
        cur = cur->next;
    }
    return out;
}

// [INTV:EDGE] declare_style(export -p처럼 `declare -x KEY="value"` 형식)과 env 스타일
// (`KEY=value`)을 하나의 함수에서 플래그로 나눠 처리한다 — export와 env 두 빌트인이 순회
// 로직(exported 변수만, 순서대로)은 완전히 같고 출력 포맷만 다르기 때문에, 순회를 두 번
// 구현하지 않고 여기 하나로 합쳤다.
int env_print(t_env *env, int declare_style)
{
    while (env != NULL) {
        if (env->key != NULL && env->exported) {
            if (declare_style
                && (shell_write_text(STDOUT_FILENO, "declare -x ") != 0
                    || shell_write_text(STDOUT_FILENO, env->key) != 0
                    || shell_write_text(STDOUT_FILENO, "=\"") != 0
                    || shell_write_text(STDOUT_FILENO, env->value) != 0
                    || shell_write_text(STDOUT_FILENO, "\"\n") != 0))
                return 1;
            if (!declare_style
                && (shell_write_text(STDOUT_FILENO, env->key) != 0
                    || shell_write_text(STDOUT_FILENO, "=") != 0
                    || shell_write_text(STDOUT_FILENO, env->value) != 0
                    || shell_write_text(STDOUT_FILENO, "\n") != 0))
                return 1;
        }
        env = env->next;
    }
    return 0;
}
