#include "str_builder.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

/* - - - - */

static const size_t str_builder_min_size = 32;

struct StrBuilder {
    char   *str;
    size_t  alloced;
    size_t  len;
};

/* - - - - */

StrBuilder *str_builder_create(void)
{
    StrBuilder *sb;

    sb          = xcalloc(1, sizeof(*sb));
    sb->str     = xmalloc(str_builder_min_size);
    *sb->str    = '\0';
    sb->alloced = str_builder_min_size;
    sb->len     = 0;

    return sb;
}

void str_builder_destroy(StrBuilder *sb)
{
    if (sb == NULL)
        return;
    free(sb->str);
    free(sb);
}

/* - - - - */

/*! Ensure there is enough space for data being added plus a NULL terminator.
 *
 * param[in,out] sb      Builder.
 * param[in]     add_len The length that needs to be added *not* including a NULL terminator.
 */
static void str_builder_ensure_space(StrBuilder *sb, size_t add_len)
{
    size_t needed;

    if (sb == NULL || add_len == 0)
        return;

    needed = size_add(size_add(sb->len, add_len), 1);
    if (sb->alloced >= needed)
        return;

    /* Doubling growth strategy, falling back to the exact size if
     * doubling would overflow. */
    while (sb->alloced < needed) {
        if (sb->alloced > SIZE_MAX / 2) {
            sb->alloced = needed;
            break;
        }
        sb->alloced <<= 1;
    }
    sb->str = xrealloc(sb->str, sb->alloced);
}

/* - - - - */

void str_builder_add_str(StrBuilder *sb, const char *str)
{
    size_t len;

    if (sb == NULL || str == NULL)
        return;

    len = strlen(str);
    str_builder_ensure_space(sb, len);
    memmove(sb->str+sb->len, str, len);
    sb->len += len;
    sb->str[sb->len] = '\0';
}

void str_builder_copy_str(StrBuilder *sb, const char *str, size_t start, size_t end)
{
    size_t len;
    if (sb == NULL || str == NULL || end <= start)
        return;

    len = end - start;
    str_builder_ensure_space(sb, len);
    memmove(sb->str+sb->len, str + start, len);
    sb->len += len;
    sb->str[sb->len] = '\0';
}

void str_builder_add_char(StrBuilder *sb, char c)
{
    if (sb == NULL)
        return;
    str_builder_ensure_space(sb, 1);
    sb->str[sb->len] = c;
    sb->len++;
    sb->str[sb->len] = '\0';
}

void str_builder_add_format(StrBuilder *sb, const char *format, ...)
{
    va_list args;
    va_list args_copy;
    int len;

    if (sb == NULL || format == NULL)
        return;

    va_start(args, format);
    va_copy(args_copy, args);
    len = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (len > 0) {
        str_builder_ensure_space(sb, (size_t)len);
        vsnprintf(sb->str + sb->len, (size_t)len + 1, format, args);
        sb->len += (size_t)len;
    }
    va_end(args);
}

/* - - - - */

void str_builder_clear(StrBuilder *sb)
{
    if (sb == NULL)
        return;
    str_builder_truncate(sb, 0);
}

void str_builder_truncate(StrBuilder *sb, size_t len)
{
    if (sb == NULL || len >= sb->len)
        return;

    sb->len = len;
    sb->str[sb->len] = '\0';
}

/* - - - - */

size_t str_builder_len(const StrBuilder *sb)
{
    if (sb == NULL)
        return 0;
    return sb->len;
}

const char *str_builder_peek(const StrBuilder *sb)
{
    if (sb == NULL)
        return NULL;
    return sb->str;
}

char *str_builder_release(StrBuilder *sb, size_t *len)
{
    char *out;

    if (sb == NULL)
        return NULL;

    if (len != NULL)
        *len = sb->len;
    out = sb->str;
    free(sb);
    return out;
}

char str_builder_char_from_end(const StrBuilder *sb, size_t n)
{
    if (sb == NULL || n == 0 || n > sb->len)
        return '\0';

    return sb->str[sb->len - n];
}
