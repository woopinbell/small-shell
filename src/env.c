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

const char *env_get(t_env *env, const char *key)
{
    t_env *node;

    node = env_find(env, key);
    if (node == NULL)
        return "";
    return node->value;
}

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
