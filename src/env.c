#include "shell.h"

#include <stdlib.h>
#include <string.h>

static t_env *env_new(const char *key, const char *value, int exported) {
    t_env *node = (t_env *)sh_xcalloc(1, sizeof(t_env));
    node->key = sh_strdup(key);
    node->value = sh_strdup(value ? value : "");
    node->exported = exported;
    return node;
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
