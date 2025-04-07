#include "shell.h"
#include "string_builder.h"

#include <stdio.h>
#include <stdlib.h>

#define LITERAL_MARK '\001'

static int append_status(t_string_builder *out, int status)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "%d", status);
    return string_builder_append_text(out, buf);
}

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
