#include "shell.h"

#include <stdlib.h>
#include <string.h>

static t_env *env_new(const char *key, const char *value, int exported) {
    t_env *node = (t_env *)sh_xcalloc(1, sizeof(t_env));
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
    t_env *head = NULL;
    t_env *tail = NULL;
    size_t i;

    if (!envp)
        return NULL;
    for (i = 0; envp[i]; i++) {
        const char *eq = strchr(envp[i], '=');
        char *key;
        t_env *node;

        if (!eq)
            continue;
        key = sh_substr(envp[i], 0, (size_t)(eq - envp[i]));
        node = env_new(key, eq + 1, 1);
        free(key);
        if (!head)
            head = node;
        else
            tail->next = node;
        tail = node;
    }
    return head;
}

void env_free(t_env *env) {
    while (env) {
        t_env *next = env->next;
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
