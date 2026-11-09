#pragma once

#include <stddef.h>

/*! addtogroup str_builder String Builder
 * https://nachtimwald.com/2017/02/26/efficient-c-string-builder/
 * A mutable string of characters used to dynamically build a string.
 *
 * @{
 */

struct StrBuilder;
typedef struct StrBuilder StrBuilder;

/* - - - - */

/*! Create a str builder.
 *
 * return str builder.
 */
StrBuilder *str_builder_create(void);

/*! Destroy a str builder.
 *
 * param[in,out] sb Builder.
 */
void str_builder_destroy(StrBuilder *sb);

/* - - - - */

/*! Add a NUL-terminated string to the builder. NULL adds nothing.
 *
 * param[in,out] sb  Builder.
 * param[in]     str String to add.
 */
void str_builder_add_str(StrBuilder *sb, const char *str);

/*! Add str[start, end) to the builder. Nothing is added if end <= start.
 */
void str_builder_copy_str(StrBuilder *sb, const char *str, size_t start, size_t end);

/*! Add a character to the builder.
 *
 * param[in,out] sb Builder.
 * param[in]     c  Character.
 */
void str_builder_add_char(StrBuilder *sb, char c);

#if defined(__GNUC__) || defined(__clang__)
# define STR_BUILDER_PRINTF(format_index, args_index) \
    __attribute__((format(printf, format_index, args_index)))
#else
# define STR_BUILDER_PRINTF(format_index, args_index)
#endif

/*! Add text formatted as by printf to the builder.
 *
 * param[in,out] sb     Builder.
 * param[in]     format printf format string, followed by its arguments.
 */
void str_builder_add_format(StrBuilder *sb, const char *format, ...)
    STR_BUILDER_PRINTF(2, 3);

/* - - - - */

/*! Clear the builder.
 *
 * param[in,out] sb  Builder.
 */
void str_builder_clear(StrBuilder *sb);

/*! Remove data from the end of the builder.
 *
 * param[in,out] sb  Builder.
 * param[in]     len The new length of the string.
 *                    Anything after this length is removed.
 */
void str_builder_truncate(StrBuilder *sb, size_t len);

/* - - - - */

/*! The length of the string contained in the builder.
 *
 * param[in] sb Builder.
 *
 * return Length.
 */
size_t str_builder_len(const StrBuilder *sb);

/*! A pointer to the internal buffer with the builder's string data.
 *
 * The data is guaranteed to be NULL terminated.
 *
 * param[in] sb Builder.
 *
 * return Pointer to internal string data.
 */
const char *str_builder_peek(const StrBuilder *sb);

/*! Destroy the builder but keep its string.
 *
 * It hands over the buffer without copying it.
 *
 * param[in]  sb  Builder. It is freed and must not be used again.
 * param[out] len Length of returned data. Can be NULL if not needed.
 *
 * return The builder's string. The caller owns it and must free() it.
 */
char *str_builder_release(StrBuilder *sb, size_t *len);

/*! The character n places from the end of the string (1 is the last), or
 * NUL if the string is shorter than that.
 */
char str_builder_char_from_end(const StrBuilder *sb, size_t n);

/*! @}
 */