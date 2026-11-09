#pragma once

#include <stdbool.h>
#include "../support/str_builder.h"

/*
 * Builds JavaScript (and TypeScript) source text with indentation.
 * Create with js_builder_create and free with js_builder_destroy, or with
 * js_builder_release to keep the text.
 */
typedef struct JSBuilder {
  char* indent_str;
  size_t indent_len;

  str_builder_t* sb;
  str_builder_t* ib;
} JSBuilder;

JSBuilder* js_builder_create(void);
void js_builder_destroy(JSBuilder*);

void js_builder_add_char(JSBuilder*, char);
void js_builder_add_str(JSBuilder*, const char*);
void js_builder_copy_str(JSBuilder*, const char*, size_t, size_t);
void js_builder_safe_key(JSBuilder*, char*);
void js_builder_add_string(JSBuilder*, char*);
void js_builder_copy_string(JSBuilder*, const char*, size_t, size_t);

void js_builder_add_indent(JSBuilder*);
void js_builder_increase_indent(JSBuilder*);
void js_builder_decrease_indent(JSBuilder*);
void js_builder_start_object(JSBuilder*);
void js_builder_end_object(JSBuilder*);
void js_builder_start_prop(JSBuilder*, char*);
void js_builder_start_prop_no_copy(JSBuilder*, const char*, size_t, size_t);
void js_builder_shorthand_prop(JSBuilder*, char*);
void js_builder_start_call(JSBuilder*, char*);
void js_builder_end_call(JSBuilder*);
void js_builder_start_array(JSBuilder*, bool);
void js_builder_end_array(JSBuilder*, bool);
void js_builder_add_export(JSBuilder*);
void js_builder_add_const(JSBuilder*, char*);
void js_builder_add_arg(JSBuilder*, char*);

/* The text so far. It belongs to the builder and changes as it grows. */
const char* js_builder_peek(JSBuilder*);
/* Free the builder and return its text, which the caller must free(). */
char* js_builder_release(JSBuilder*);