#pragma once

#include <stddef.h>

#include "../support/str_builder.h"

/*
 * Builds indented JavaScript (or TypeScript) text a line at a time.
 * Create with js_builder_create and free with js_builder_destroy, or with
 * js_builder_release to keep the text.
 */
typedef struct JSBuilder {
  StrBuilder* text;
  // How many levels the next line is indented, two spaces each.
  size_t depth;
} JSBuilder;

JSBuilder* js_builder_create(void);
void js_builder_destroy(JSBuilder*);

/* A whole line: the indentation, the text formatted as by printf, and a
 * newline. */
void js_builder_line(JSBuilder*, const char* format, ...) STR_BUILDER_PRINTF(2, 3);
/* An empty line. */
void js_builder_blank(JSBuilder*);
/* A line that opens a block (it ends in `{`): the lines after it are
 * indented one level more. */
void js_builder_open(JSBuilder*, const char* format, ...) STR_BUILDER_PRINTF(2, 3);
/* A line that closes a block, such as "}" or "});", one level back out. */
void js_builder_close(JSBuilder*, const char* format, ...) STR_BUILDER_PRINTF(2, 3);

/* Text added to the current line, with no indentation or newline. */
void js_builder_add_str(JSBuilder*, const char* str);
void js_builder_add_format(JSBuilder*, const char* format, ...) STR_BUILDER_PRINTF(2, 3);
/* The indentation alone, to start a line that is then built with add_str. */
void js_builder_add_indent(JSBuilder*);

/* The text so far. It belongs to the builder and changes as it grows. */
const char* js_builder_peek(const JSBuilder*);
/* Free the builder and return its text, which the caller must free(). */
char* js_builder_release(JSBuilder*);
