#include "string_builder.h"
#include "runtime.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define STRING_BUILDER_INITIAL_CAPACITY 64

int string_builder_init(t_string_builder *builder)
{
    if (builder == NULL) {
        errno = EINVAL;
        return 1;
    }
    builder->data = (char *)shell_malloc(STRING_BUILDER_INITIAL_CAPACITY);
    builder->length = 0;
    builder->capacity = 0;
    if (builder->data == NULL)
        return 1;
    builder->capacity = STRING_BUILDER_INITIAL_CAPACITY;
    builder->data[0] = '\0';
    return 0;
}

void string_builder_discard(t_string_builder *builder)
{
    if (builder == NULL)
        return;
    free(builder->data);
    builder->data = NULL;
    builder->length = 0;
    builder->capacity = 0;
}

static int string_builder_reserve(t_string_builder *builder, size_t extra)
{
    size_t  needed;
    size_t  capacity;
    char    *grown;

    if (extra > SIZE_MAX - builder->length - 1) {
        errno = ENOMEM;
        return 1;
    }
    needed = builder->length + extra + 1;
    if (needed <= builder->capacity)
        return 0;
    capacity = builder->capacity;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    grown = (char *)shell_realloc(builder->data, capacity);
    if (grown == NULL)
        return 1;
    builder->data = grown;
    builder->capacity = capacity;
    return 0;
}

int string_builder_append_char(t_string_builder *builder, char value)
{
    if (builder == NULL || builder->data == NULL) {
        errno = EINVAL;
        return 1;
    }
    if (string_builder_reserve(builder, 1) != 0)
        return 1;
    builder->data[builder->length++] = value;
    builder->data[builder->length] = '\0';
    return 0;
}

int string_builder_append_text(t_string_builder *builder, const char *text)
{
    size_t length;

    if (builder == NULL || builder->data == NULL) {
        errno = EINVAL;
        return 1;
    }
    if (text == NULL)
        return 0;
    length = strlen(text);
    if (string_builder_reserve(builder, length) != 0)
        return 1;
    memcpy(builder->data + builder->length, text, length);
    builder->length += length;
    builder->data[builder->length] = '\0';
    return 0;
}

char *string_builder_take(t_string_builder *builder)
{
    char *data;

    if (builder == NULL)
        return NULL;
    data = builder->data;
    builder->data = NULL;
    builder->length = 0;
    builder->capacity = 0;
    return data;
}
