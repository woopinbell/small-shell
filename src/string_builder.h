#ifndef STRING_BUILDER_H
# define STRING_BUILDER_H

# include <stddef.h>

typedef struct s_string_builder {
    char    *data;
    size_t  length;
    size_t  capacity;
}   t_string_builder;

int     string_builder_init(t_string_builder *builder);
void    string_builder_discard(t_string_builder *builder);
int     string_builder_append_char(t_string_builder *builder, char value);
int     string_builder_append_text(t_string_builder *builder,
            const char *text);
char    *string_builder_take(t_string_builder *builder);

#endif
