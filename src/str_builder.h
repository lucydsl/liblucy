#ifndef __STR_BUILDER_H__
#define __STR_BUILDER_H__

#include <stddef.h>

/*! addtogroup str_builder String Builder
 * https://nachtimwald.com/2017/02/26/efficient-c-string-builder/
 * A mutable string of characters used to dynamically build a string.
 *
 * @{
 */

struct str_builder;
typedef struct str_builder str_builder_t;

/* - - - - */

/*! Create a str builder.
 *
 * return str builder.
 */
str_builder_t *str_builder_create(void);

/*! Destroy a str builder.
 *
 * param[in,out] sb Builder.
 */
void str_builder_destroy(str_builder_t *sb);

/* - - - - */

/*! Add a string to the builder.
 *
 * param[in,out] sb  Builder.
 * param[in]     str String to add.
 * param[in]     len Length of string to add. If 0, strlen will be called
 *                internally to determine length.
 */
void str_builder_add_str(str_builder_t *sb, const char *str, size_t len);

/*! Add a character to the builder.
 *
 * param[in,out] sb Builder.
 * param[in]     c  Character.
 */
void str_builder_add_char(str_builder_t *sb, char c);

/*! Add an integer as to the builder.
 *
 * param[in,out] sb  Builder.
 * param[in]     val Int to add.
 */
void str_builder_add_int(str_builder_t *sb, int val);

/* - - - - */

/*! Clear the builder.
 *
 * param[in,out] sb  Builder.
 */
void str_builder_clear(str_builder_t *sb);

/*! Remove data from the end of the builder.
 *
 * param[in,out] sb  Builder.
 * param[in]     len The new length of the string.
 *                    Anything after this length is removed.
 */
void str_builder_truncate(str_builder_t *sb, size_t len);

/*! Remove data from the beginning of the builder.
 *
 * param[in,out] sb  Builder.
 * param[in]     len The length to remove.
 */
void str_builder_drop(str_builder_t *sb, size_t len);

/* - - - - */

/*! The length of the string contained in the builder.
 *
 * param[in] sb Builder.
 *
 * return Length.
 */
size_t str_builder_len(const str_builder_t *sb);

/*! A pointer to the internal buffer with the builder's string data.
 *
 * The data is guaranteed to be NULL terminated.
 *
 * param[in] sb Builder.
 *
 * return Pointer to internal string data.
 */
const char *str_builder_peek(const str_builder_t *sb);

/*! Return a copy of the string data.
 *
 * param[in]  sb  Builder.
 * param[out] len Length of returned data. Can be NULL if not needed.
 *
 * return Copy of the internal string data.
 */
char *str_builder_dump(const str_builder_t *sb, size_t *len);

/*! @}
 */

#endif /* __STR_BUILDER_H__ */