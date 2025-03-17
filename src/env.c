#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static t_env *env_new(const char *key, const char *value, int exported) {
    t_env *node;

    node = (t_env *)sh_xcalloc(1, sizeof(t_env));
    node->key = sh_strdup(key);
    node->value = sh_strdup(value ? value : "");
    node->exported = exported ? 1 : 0;
    return node;
}

static t_env *env_find(t_env *env, const char *key) {
    while (env) {
        if (env->key && key && strcmp(env->key, key) == 0)
            return env;
        env = env->next;
    }
    return NULL;
}

int shell_env_is_valid_name(const char *key) {
    size_t i;

    if (!key || !sh_is_name_start((unsigned char)key[0]))
        return 0;
    i = 1;
    while (key[i]) {
        if (!sh_is_name_char((unsigned char)key[i]))
            return 0;
        i++;
    }
    return 1;
}

t_env *env_from_environ(char **envp) {
    t_env   *head;
    t_env   *tail;
    size_t  i;
    char    *eq;
    char    *key;
    t_env   *node;

    head = NULL;
    tail = NULL;
    i = 0;
    while (envp && envp[i]) {
        eq = strchr(envp[i], '=');
        if (eq) {
            key = sh_substr(envp[i], 0, (size_t)(eq - envp[i]));
            node = env_new(key, eq + 1, 1);
            free(key);
            if (!head)
                head = node;
            else
                tail->next = node;
            tail = node;
        }
        i++;
    }
    return head;
}

void env_free(t_env *env) {
    t_env *next;

    while (env) {
        next = env->next;
        free(env->key);
        free(env->value);
        free(env);
        env = next;
    }
}

const char *env_get(t_env *env, const char *key) {
    t_env *node;

    node = env_find(env, key);
    if (!node)
        return "";
    return node->value;
}

int env_set(t_env **env, const char *key, const char *value, int exported) {
    t_env *node;
    t_env *tail;

    if (!env || !shell_env_is_valid_name(key))
        return 1;
    node = env_find(*env, key);
    if (node) {
        if (value) {
            free(node->value);
            node->value = sh_strdup(value);
        }
        if (exported)
            node->exported = 1;
        return 0;
    }
    node = env_new(key, value ? value : "", exported);
    if (!*env) {
        *env = node;
        return 0;
    }
    tail = *env;
    while (tail->next)
        tail = tail->next;
    tail->next = node;
    return 0;
}

int env_unset(t_env **env, const char *key) {
    t_env *cur;
    t_env *prev;

    if (!env || !key)
        return 1;
    cur = *env;
    prev = NULL;
    while (cur) {
        if (cur->key && strcmp(cur->key, key) == 0) {
            if (prev)
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

char **env_to_environ(t_env *env) {
    size_t  count;
    size_t  i;
    char    **out;
    char    *pair;
    t_env   *cur;

    count = 0;
    cur = env;
    while (cur) {
        if (cur->key && cur->exported)
            count++;
        cur = cur->next;
    }
    out = (char **)sh_xcalloc(count + 1, sizeof(char *));
    i = 0;
    cur = env;
    while (cur) {
        if (cur->key && cur->exported) {
            pair = sh_strjoin_free(sh_strdup(cur->key), "=");
            pair = sh_strjoin_free(pair, cur->value);
            out[i++] = pair;
        }
        cur = cur->next;
    }
    return out;
}

void env_print(t_env *env, int declare_style) {
    while (env) {
        if (env->key && env->exported) {
            if (declare_style)
                printf("declare -x %s=\"%s\"\n", env->key, env->value);
            else
                printf("%s=%s\n", env->key, env->value);
        }
        env = env->next;
    }
}

static int is_sentinel(t_env *env) {
    return (env && !env->key && !env->value && env->exported == 0);
}

static t_env *env_head(t_env *env) {
    if (is_sentinel(env))
        return env->next;
    return env;
}

static const t_env *env_head_const(const t_env *env) {
    if (env && !env->key && !env->value && env->exported == 0)
        return env->next;
    return env;
}

int shell_env_init(t_env *env, char **envp) {
    if (!env)
        return 1;
    env->key = NULL;
    env->value = NULL;
    env->exported = 0;
    env->next = env_from_environ(envp);
    return 0;
}

void shell_env_free(t_env *env) {
    if (!env)
        return;
    if (is_sentinel(env)) {
        env_free(env->next);
        env->next = NULL;
        return;
    }
    env_free(env);
}

const char *shell_env_get(const t_env *env, const char *key) {
    const t_env *node;

    node = env_head_const(env);
    while (node) {
        if (node->key && key && strcmp(node->key, key) == 0)
            return node->value;
        node = node->next;
    }
    return NULL;
}

int shell_env_set(t_env *env, const char *key, const char *value, int exported) {
    t_env *head;

    if (!env)
        return 1;
    if (is_sentinel(env))
        return env_set(&env->next, key, value, exported);
    head = env;
    return env_set(&head, key, value, exported);
}

int shell_env_unset(t_env *env, const char *key) {
    t_env *head;

    if (!env)
        return 1;
    if (is_sentinel(env))
        return env_unset(&env->next, key);
    head = env;
    return env_unset(&head, key);
}

char **shell_env_export_list(t_env *env) {
    return env_to_environ(env_head(env));
}

char **shell_env_to_envp(t_env *env) {
    return env_to_environ(env_head(env));
}
