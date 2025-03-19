#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LITERAL_MARK '\001'

static char *append_char(char *out, char c) {
    char buf[2];
    buf[0] = c;
    buf[1] = '\0';
    return sh_strjoin_free(out, buf);
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
