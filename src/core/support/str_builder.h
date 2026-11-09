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

/*! Add a string to the builder.
 *
 * param[in,out] sb  Builder.
 * param[in]     str String to add.
 * param[in]     len Length of string to add. If 0, strlen will be called
 *                internally to determine length.
 */
void str_builder_add_str(StrBuilder *sb, const char *str, size_t len);

/*! Add str[start, end) to the builder. Nothing is added if end <= start.
 */
void str_builder_copy_str(StrBuilder *sb, const char *str, size_t start, size_t end);

/*! Add a character to the builder.
 *
 * param[in,out] sb Builder.
 * param[in]     c  Character.
 */
void str_builder_add_char(StrBuilder *sb, char c);

/*! Add an integer as to the builder.
 *
 * param[in,out] sb  Builder.
 * param[in]     val Int to add.
 */
void str_builder_add_int(StrBuilder *sb, int val);

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

/*! Remove data from the beginning of the builder.
 *
 * param[in,out] sb  Builder.
 * param[in]     len The length to remove.
 */
void str_builder_drop(StrBuilder *sb, size_t len);

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

/*! Return a copy of the string data.
 *
 * The copy is malloc'd and the caller must free() it. To read the data
 * without copying use str_builder_peek.
 *
 * param[in]  sb  Builder.
 * param[out] len Length of returned data. Can be NULL if not needed.
 *
 * return Copy of the internal string data.
 */
char *str_builder_dump(const StrBuilder *sb, size_t *len);

/*! Destroy the builder but keep its string.
 *
 * Use this instead of dump + destroy when the builder isn't needed any
 * more: it hands over the buffer without copying it.
 *
 * param[in]  sb  Builder. It is freed and must not be used again.
 * param[out] len Length of returned data. Can be NULL if not needed.
 *
 * return The builder's string. The caller owns it and must free() it.
 */
char *str_builder_release(StrBuilder *sb, size_t *len);

/*! The character at index, or NUL if index is outside the string
 * (negative indexes included).
 */
char str_builder_char_at(const StrBuilder *sb, long index);

/*! @}
 */