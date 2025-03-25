#define _POSIX_C_SOURCE 200809L

#include "exec_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LITERAL_MARK '\001'

struct strbuf {
    char    *data;
    size_t  len;
    size_t  cap;
};

static int sb_init(struct strbuf *buf)
{
    buf->cap = 64;
    buf->len = 0;
    buf->data = (char *)malloc(buf->cap);
    if (buf->data == NULL)
        return 1;
    buf->data[0] = '\0';
    return 0;
}

static void sb_free(struct strbuf *buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

static int sb_reserve(struct strbuf *buf, size_t extra)
{
    size_t  needed;
    char    *next;

    needed = buf->len + extra + 1;
    if (needed <= buf->cap)
        return 0;
    while (buf->cap < needed)
        buf->cap *= 2;
    next = (char *)realloc(buf->data, buf->cap);
    if (next == NULL)
        return 1;
    buf->data = next;
    return 0;
}

static int sb_push(struct strbuf *buf, char ch)
{
    if (sb_reserve(buf, 1) != 0)
        return 1;
    buf->data[buf->len++] = ch;
    buf->data[buf->len] = '\0';
    return 0;
}

char *dequote_runtime_word(const char *word)
{
    struct strbuf   out;
    size_t          i;

    if (sb_init(&out) != 0)
        return NULL;
    i = 0;
    while (word != NULL && word[i] != '\0') {
        if (word[i] == LITERAL_MARK && word[i + 1] != '\0') {
            if (sb_push(&out, word[i + 1]) != 0) {
                sb_free(&out);
                return NULL;
            }
            i += 2;
        } else {
            if (sb_push(&out, word[i]) != 0) {
                sb_free(&out);
                return NULL;
            }
            i++;
        }
    }
    return out.data;
}

int add_heredoc_entry(struct exec_context *ctx, const t_redir *redir,
    char *body)
{
    struct heredoc_entry *entry;

    entry = (struct heredoc_entry *)malloc(sizeof(*entry));
    if (entry == NULL)
        return 1;
    entry->redir = redir;
    entry->body = body;
    entry->next = ctx->heredocs;
    ctx->heredocs = entry;
    return 0;
}

void exec_heredoc_entries_free(struct heredoc_entry *entry)
{
    struct heredoc_entry *next;

    while (entry != NULL) {
        next = entry->next;
        free(entry->body);
        free(entry);
        entry = next;
    }
}

const char *exec_find_heredoc_body(const struct exec_context *ctx,
    const t_redir *redir)
{
    struct heredoc_entry *entry;

    entry = ctx->heredocs;
    while (entry != NULL) {
        if (entry->redir == redir)
            return entry->body;
        entry = entry->next;
    }
    return "";
}
