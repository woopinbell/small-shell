#include "shell.h"

#include <stdlib.h>

#define LITERAL_MARK '\001'

static t_token *new_token(t_token_type type, char *text, size_t start) {
    t_token *token = (t_token *)sh_xcalloc(1, sizeof(t_token));
    token->type = type;
    token->text = text ? text : sh_strdup("");
    token->start = start;
    return token;
}

static void push_token(t_token **head, t_token **tail, t_token *node) {
    if (!*head)
        *head = node;
    else
        (*tail)->next = node;
    *tail = node;
}

static int is_operator_char(char c) {
    return (c == '|' || c == '<' || c == '>' || c == '&' || c == ';');
}

static int is_shell_space(char c) {
    return (c == ' ' || c == '\t' || c == '\n'
        || c == '\r' || c == '\v' || c == '\f');
}

static char *append_char(char *word, char c) {
    char buf[2];

    buf[0] = c;
    buf[1] = '\0';
    return sh_strjoin_free(word, buf);
}

static char *append_literal(char *word, char c) {
    word = append_char(word, LITERAL_MARK);
    return append_char(word, c);
}

static char *read_word(const char *line, size_t *i, char **error) {
    char    *word;
    char    quote;

    word = sh_strdup("");
    while (line[*i] && !is_shell_space(line[*i])
        && !is_operator_char(line[*i])) {
        if (line[*i] == '\'' || line[*i] == '"') {
            quote = line[*i];
            (*i)++;
            while (line[*i] && line[*i] != quote) {
                if (quote == '\'')
                    word = append_literal(word, line[*i]);
                else
                    word = append_char(word, line[*i]);
                (*i)++;
            }
            if (!line[*i]) {
                free(word);
                *error = sh_strdup("syntax error: unclosed quote");
                return NULL;
            }
            (*i)++;
        } else {
            word = append_char(word, line[*i]);
            (*i)++;
        }
    }
    return word;
}

t_token *tokenize_line(const char *line, char **error) {
    t_token *head;
    t_token *tail;
    size_t  i;
    size_t  start;
    char    *word;

    head = NULL;
    tail = NULL;
    i = 0;
    if (error)
        *error = NULL;
    while (line && line[i]) {
        while (is_shell_space(line[i]))
            i++;
        if (!line[i])
            break;
        if (is_operator_char(line[i])) {
            if (error)
                *error = sh_strdup("syntax error: unsupported operator");
            free_tokens(head);
            return NULL;
        }
        start = i;
        word = read_word(line, &i, error);
        if (!word) {
            free_tokens(head);
            return NULL;
        }
        push_token(&head, &tail, new_token(TOK_WORD, word, start));
    }
    return head;
}

void free_tokens(t_token *tokens) {
    t_token *next;

    while (tokens) {
        next = tokens->next;
        free(tokens->text);
        free(tokens);
        tokens = next;
    }
}
