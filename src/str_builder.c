#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "str_builder.h"

/* - - - - */

static const size_t str_builder_min_size = 32;

struct str_builder {
    char   *str;
    size_t  alloced;
    size_t  len;
};

/* - - - - */

str_builder_t *str_builder_create(void)
{
    str_builder_t *sb;

    sb          = calloc(1, sizeof(*sb));
    sb->str     = malloc(str_builder_min_size);
    *sb->str    = '\0';
    sb->alloced = str_builder_min_size;
    sb->len     = 0;

    return sb;
}

void str_builder_destroy(str_builder_t *sb)
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
static void str_builder_ensure_space(str_builder_t *sb, size_t add_len)
{
    if (sb == NULL || add_len == 0)
        return;

    if (sb->alloced >= sb->len+add_len+1)
        return;

    while (sb->alloced < sb->len+add_len+1) {
        /* Doubling growth strategy. */
        sb->alloced <<= 1;
        if (sb->alloced == 0) {
            /* Left shift of max bits will go to 0. An unsigned type set to
             * -1 will return the maximum possible size. However, we should
             *  have run out of memory well before we need to do this. Since
             *  this is the theoretical maximum total system memory we don't
             *  have a flag saying we can't grow any more because it should
             *  be impossible to get to this point. */
            sb->alloced--;
        }
    }
    sb->str = realloc(sb->str, sb->alloced);
}

/* - - - - */

void str_builder_add_str(str_builder_t *sb, const char *str, size_t len)
{
    if (sb == NULL || str == NULL || *str == '\0')
        return;

    if (len == 0)
        len = strlen(str);

    str_builder_ensure_space(sb, len);
    memmove(sb->str+sb->len, str, len);
    sb->len += len;
    sb->str[sb->len] = '\0';
}

void str_builder_add_char(str_builder_t *sb, char c)
{
    if (sb == NULL)
        return;
    str_builder_ensure_space(sb, 1);
    sb->str[sb->len] = c;
    sb->len++;
    sb->str[sb->len] = '\0';
}

void str_builder_add_int(str_builder_t *sb, int val)
{
    char str[12];

    if (sb == NULL)
        return;

    snprintf(str, sizeof(str), "%d", val);
    str_builder_add_str(sb, str, 0);
}

/* - - - - */

void str_builder_clear(str_builder_t *sb)
{
    if (sb == NULL)
        return;
    str_builder_truncate(sb, 0);
}

void str_builder_truncate(str_builder_t *sb, size_t len)
{
    if (sb == NULL || len >= sb->len)
        return;

    sb->len = len;
    sb->str[sb->len] = '\0';
}

void str_builder_drop(str_builder_t *sb, size_t len)
{
    if (sb == NULL || len == 0)
        return;

    if (len >= sb->len) {
        str_builder_clear(sb);
        return;
    }

    sb->len -= len;
    /* +1 to move the NULL. */
    memmove(sb->str, sb->str+len, sb->len+1);
}

/* - - - - */

size_t str_builder_len(const str_builder_t *sb)
{
    if (sb == NULL)
        return 0;
    return sb->len;
}

const char *str_builder_peek(const str_builder_t *sb)
{
    if (sb == NULL)
        return NULL;
    return sb->str;
}

char *str_builder_dump(const str_builder_t *sb, size_t *len)
{
    char *out;

    if (sb == NULL)
        return NULL;

    if (len != NULL)
        *len = sb->len;
    out = malloc(sb->len+1);
    memcpy(out, sb->str, sb->len+1);
    return out;
}