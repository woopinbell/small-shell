#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LITERAL_MARK '\001'

static char *append_status(char *out, int status) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", status);
    return sh_strjoin_free(out, buf);
}

static char *append_char(char *out, char c) {
    char buf[2];
    buf[0] = c;
    buf[1] = '\0';
    return sh_strjoin_free(out, buf);
}

char *expand_word(t_shell *shell, const char *word) {
    char    *out;
    size_t  i;
    size_t  start;
    char    *key;

    out = sh_strdup("");
    i = 0;
    while (word && word[i]) {
        if (word[i] == LITERAL_MARK && word[i + 1]) {
            out = append_char(out, word[i + 1]);
            i += 2;
        } else if (word[i] == '$' && word[i + 1] == '?') {
            out = append_status(out, shell->last_status);
            i += 2;
        } else if (word[i] == '$' && sh_is_name_start((unsigned char)word[i + 1])) {
            start = i + 1;
            i = start + 1;
            while (sh_is_name_char((unsigned char)word[i]))
                i++;
            key = sh_substr(word, start, i - start);
            out = sh_strjoin_free(out, env_get(shell->env, key));
            free(key);
        } else {
            out = append_char(out, word[i]);
            i++;
        }
    }
    return out;
}

static char *dequote_word(const char *word) {
    char    *out;
    size_t  i;

    out = sh_strdup("");
    i = 0;
    while (word && word[i]) {
        if (word[i] == LITERAL_MARK && word[i + 1]) {
            out = append_char(out, word[i + 1]);
            i += 2;
        } else {
            out = append_char(out, word[i]);
            i++;
        }
    }
    return out;
}

int shell_dequote_word(const char *word, char **out, char **error) {
    if (error)
        *error = NULL;
    if (!out) {
        if (error)
            *error = sh_strdup("dequote output is null");
        return 1;
    }
    *out = dequote_word(word);
    return 0;
}

static int expand_words(t_shell *shell, char ***words) {
    size_t  i;
    char    *expanded;

    i = 0;
    while (*words && (*words)[i]) {
        expanded = expand_word(shell, (*words)[i]);
        free((*words)[i]);
        (*words)[i] = expanded;
        i++;
    }
    return 0;
}

int expand_pipeline(t_shell *shell, t_pipeline *pipeline) {
    t_command   *cmd;
    t_redir     *redir;
    char        *expanded;

    while (pipeline) {
        cmd = pipeline->commands;
        while (cmd) {
            expand_words(shell, &cmd->argv);
            redir = cmd->redirs;
            while (redir) {
                expanded = expand_word(shell, redir->target);
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
        int last_status, char **error) {
    t_shell shell;

    if (error)
        *error = NULL;
    if (!sequence)
        return 0;
    shell.env = (t_env *)env;
    shell.last_status = last_status;
    shell.running = 1;
    return expand_pipeline(&shell, sequence->pipelines);
}
