#include "shell.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *sh_xcalloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (!ptr) {
        perror("small-shell: calloc");
        exit(1);
    }
    return ptr;
}

char *sh_strdup(const char *s) {
    char *copy;
    size_t len;

    if (!s)
        s = "";
    len = strlen(s);
    copy = (char *)malloc(len + 1);
    if (!copy) {
        perror("small-shell: malloc");
        exit(1);
    }
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

char *sh_substr(const char *s, size_t start, size_t len) {
    char *out;
    size_t n;

    out = (char *)sh_xcalloc(len + 1, sizeof(char));
    n = 0;
    while (n < len && s[start + n]) {
        out[n] = s[start + n];
        n++;
    }
    out[n] = '\0';
    return out;
}

char *sh_strjoin_free(char *left, const char *right) {
    size_t  a;
    size_t  b;
    char    *out;

    if (!left)
        left = sh_strdup("");
    if (!right)
        right = "";
    a = strlen(left);
    b = strlen(right);
    out = (char *)sh_xcalloc(a + b + 1, sizeof(char));
    memcpy(out, left, a);
    memcpy(out + a, right, b);
    free(left);
    return out;
}

void sh_free_words(char **words) {
    size_t i;

    if (!words)
        return;
    i = 0;
    while (words[i]) {
        free(words[i]);
        i++;
    }
    free(words);
}

char *shell_strndup(const char *s, size_t len) {
    char *out;
    size_t i;

    if (!s)
        s = "";
    out = (char *)sh_xcalloc(len + 1, sizeof(char));
    i = 0;
    while (i < len && s[i]) {
        out[i] = s[i];
        i++;
    }
    out[i] = '\0';
    return out;
}

char *shell_itoa_status(int status) {
    char    buf[32];
    long    value;
    size_t  len;
    char    *out;

    value = status;
    len = 0;
    if (value < 0) {
        buf[len++] = '-';
        value = -value;
    }
    if (value == 0)
        buf[len++] = '0';
    else {
        char digits[24];
        size_t count;

        count = 0;
        while (value > 0) {
            digits[count++] = (char)('0' + (value % 10));
            value /= 10;
        }
        while (count > 0)
            buf[len++] = digits[--count];
    }
    buf[len] = '\0';
    out = (char *)malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, buf, len + 1);
    return out;
}

void shell_strv_free(char **words) {
    sh_free_words(words);
}

int sh_is_name_start(int c) {
    return (isalpha((unsigned char)c) || c == '_');
}

int sh_is_name_char(int c) {
    return (isalnum((unsigned char)c) || c == '_');
}
