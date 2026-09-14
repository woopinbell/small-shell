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

// [INTV:PERF] - [TRAP] 용량을 1씩 늘리지 않고 배로(*2) 키운다 — 토큰 하나를 한 글자씩
// append하는 호출부(token.c read_word 등)가 매번 realloc하지 않도록 상환 O(1) 성장을 보장한다.
// 다만 needed가 이미 capacity*2보다 크면(한 번에 큰 텍스트를 append) capacity를 needed까지
// 바로 끌어올린다 — 그렇지 않으면 필요한 크기를 넘을 때까지 배로 늘리는 루프를 여러 번 돌아야
// 한다. extra 오버플로 체크(SIZE_MAX - length - 1)가 배로 늘리기 전에 있어야 capacity *= 2
// 자체가 SIZE_MAX를 넘어 wrap되는 걸 막는다.
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
