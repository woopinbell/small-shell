#include "shell.h"
#include "runtime.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void *sh_calloc(size_t count, size_t size)
{
    return shell_calloc(count, size);
}

char *sh_strdup(const char *s)
{
    char    *copy;
    size_t  len;

    if (s == NULL)
        s = "";
    len = strlen(s);
    if (len == SIZE_MAX)
        return NULL;
    copy = (char *)shell_malloc(len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

char *sh_substr(const char *s, size_t start, size_t len)
{
    char    *out;
    size_t  n;

    if (s == NULL || len == SIZE_MAX)
        return NULL;
    out = (char *)shell_calloc(len + 1, sizeof(char));
    if (out == NULL)
        return NULL;
    n = 0;
    while (n < len && s[start + n] != '\0') {
        out[n] = s[start + n];
        n++;
    }
    return out;
}

char *sh_strjoin_free(char *left, const char *right)
{
    size_t  a;
    size_t  b;
    char    *out;

    if (left == NULL)
        return NULL;
    if (right == NULL)
        right = "";
    a = strlen(left);
    b = strlen(right);
    if (b > SIZE_MAX - a - 1) {
        free(left);
        return NULL;
    }
    out = (char *)shell_malloc(a + b + 1);
    if (out == NULL) {
        free(left);
        return NULL;
    }
    memcpy(out, left, a);
    memcpy(out + a, right, b + 1);
    free(left);
    return out;
}

void sh_free_words(char **words)
{
    size_t i;

    if (words == NULL)
        return;
    i = 0;
    while (words[i] != NULL) {
        free(words[i]);
        i++;
    }
    free(words);
}

char *shell_strndup(const char *s, size_t len)
{
    char    *out;
    size_t  i;

    if (s == NULL || len == SIZE_MAX)
        return NULL;
    out = (char *)shell_calloc(len + 1, sizeof(char));
    if (out == NULL)
        return NULL;
    i = 0;
    while (i < len && s[i] != '\0') {
        out[i] = s[i];
        i++;
    }
    return out;
}

char *shell_itoa_status(int status)
{
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
        char    digits[24];
        size_t  count;

        count = 0;
        while (value > 0) {
            digits[count++] = (char)('0' + (value % 10));
            value /= 10;
        }
        while (count > 0)
            buf[len++] = digits[--count];
    }
    out = (char *)shell_malloc(len + 1);
    if (out == NULL)
        return NULL;
    memcpy(out, buf, len);
    out[len] = '\0';
    return out;
}

void shell_strv_free(char **words)
{
    sh_free_words(words);
}

int sh_is_name_start(int c)
{
    return (isalpha((unsigned char)c) || c == '_');
}

int sh_is_name_char(int c)
{
    return (isalnum((unsigned char)c) || c == '_');
}
