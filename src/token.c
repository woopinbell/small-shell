#include "shell.h"
#include "string_builder.h"

#include <stdlib.h>

#define LITERAL_MARK '\001'

static void set_error(char **error, const char *message)
{
    if (error != NULL && *error == NULL)
        *error = sh_strdup(message);
}

static t_token *new_token(t_token_type type, char *text, size_t start,
        int quoted)
{
    t_token *token;

    token = (t_token *)sh_calloc(1, sizeof(t_token));
    if (token == NULL) {
        free(text);
        return NULL;
    }
    token->type = type;
    token->text = text;
    token->start = start;
    token->quoted = quoted;
    return token;
}

static int push_token(t_token **head, t_token **tail, t_token *node)
{
    if (node == NULL)
        return 1;
    if (*head == NULL)
        *head = node;
    else
        (*tail)->next = node;
    *tail = node;
    return 0;
}

static int push_operator(t_token **head, t_token **tail, t_token_type type,
        const char *text, size_t start)
{
    char *copy;

    copy = sh_strdup(text);
    if (copy == NULL)
        return 1;
    return push_token(head, tail, new_token(type, copy, start, 0));
}

static int is_operator_char(char c)
{
    return (c == '|' || c == '<' || c == '>' || c == '&' || c == ';');
}

static int is_shell_space(char c)
{
    return (c == ' ' || c == '\t' || c == '\n'
        || c == '\r' || c == '\v' || c == '\f');
}

static int append_literal(t_string_builder *word, char c)
{
    return (string_builder_append_char(word, LITERAL_MARK) != 0
        || string_builder_append_char(word, c) != 0);
}

static char *read_word(const char *line, size_t *i, char **error,
        int *quoted)
{
    t_string_builder    word;
    char                quote;

    if (string_builder_init(&word) != 0)
        return NULL;
    *quoted = 0;
    while (line[*i] != '\0' && !is_shell_space(line[*i])
        && !is_operator_char(line[*i])) {
        if (line[*i] == '\'' || line[*i] == '"') {
            quote = line[*i];
            *quoted = 1;
            (*i)++;
            while (line[*i] != '\0' && line[*i] != quote) {
                int failed;

                if (quote == '\'')
                    failed = append_literal(&word, line[*i]);
                else
                    failed = string_builder_append_char(&word, line[*i]);
                if (failed != 0) {
                    string_builder_discard(&word);
                    return NULL;
                }
                (*i)++;
            }
            if (line[*i] == '\0') {
                string_builder_discard(&word);
                set_error(error, "syntax error: unclosed quote");
                return NULL;
            }
            (*i)++;
        } else {
            if (string_builder_append_char(&word, line[*i]) != 0) {
                string_builder_discard(&word);
                return NULL;
            }
            (*i)++;
        }
    }
    return string_builder_take(&word);
}

static int push_word(const char *line, size_t *i, char **error,
        t_token **head, t_token **tail)
{
    size_t  start;
    char    *word;
    int     quoted;

    start = *i;
    word = read_word(line, i, error, &quoted);
    if (word == NULL)
        return 1;
    return push_token(head, tail,
        new_token(TOK_WORD, word, start, quoted));
}

t_token *tokenize_line(const char *line, char **error)
{
    t_token *head;
    t_token *tail;
    size_t  i;
    int     failed;

    head = NULL;
    tail = NULL;
    i = 0;
    failed = 0;
    if (error != NULL)
        *error = NULL;
    while (line != NULL && line[i] != '\0' && !failed) {
        while (is_shell_space(line[i]))
            i++;
        if (line[i] == '\0')
            break;
        if (line[i] == '|' && line[i + 1] == '|') {
            failed = push_operator(&head, &tail, TOK_OR, "||", i);
            i += 2;
        } else if (line[i] == '|') {
            failed = push_operator(&head, &tail, TOK_PIPE, "|", i);
            i++;
        } else if (line[i] == '&' && line[i + 1] == '&') {
            failed = push_operator(&head, &tail, TOK_AND, "&&", i);
            i += 2;
        } else if (line[i] == '&') {
            set_error(error, "syntax error: unsupported operator '&'");
            free_tokens(head);
            return NULL;
        } else if (line[i] == ';') {
            failed = push_operator(&head, &tail, TOK_SEQ, ";", i);
            i++;
        } else if (line[i] == '<' && line[i + 1] == '<') {
            failed = push_operator(&head, &tail, TOK_HEREDOC, "<<", i);
            i += 2;
        } else if (line[i] == '<') {
            failed = push_operator(&head, &tail, TOK_REDIR_IN, "<", i);
            i++;
        } else if (line[i] == '>' && line[i + 1] == '>') {
            failed = push_operator(&head, &tail, TOK_REDIR_APPEND, ">>", i);
            i += 2;
        } else if (line[i] == '>') {
            failed = push_operator(&head, &tail, TOK_REDIR_OUT, ">", i);
            i++;
        } else {
            failed = push_word(line, &i, error, &head, &tail);
        }
    }
    if (failed) {
        set_error(error, "allocation failure");
        free_tokens(head);
        return NULL;
    }
    return head;
}

void free_tokens(t_token *tokens)
{
    t_token *next;

    while (tokens != NULL) {
        next = tokens->next;
        free(tokens->text);
        free(tokens);
        tokens = next;
    }
}
