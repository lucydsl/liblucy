#include "js_builder.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../support/mem.h"
#include "../support/str_builder.h"

JSBuilder* js_builder_create(void) {
  JSBuilder* jsb = xmalloc(sizeof(*jsb));
  jsb->text = str_builder_create();
  jsb->depth = 0;
  return jsb;
}

void js_builder_destroy(JSBuilder* jsb) {
  if(jsb == NULL) {
    return;
  }
  str_builder_destroy(jsb->text);
  free(jsb);
}

void js_builder_add_indent(JSBuilder* jsb) {
  for(size_t i = 0; i < jsb->depth; i++) {
    str_builder_add_str(jsb->text, "  ");
  }
}

// The indentation, then format with args, then a newline.
static void add_line(JSBuilder* jsb, const char* format, va_list args)
  STR_BUILDER_PRINTF(2, 0);

static void add_line(JSBuilder* jsb, const char* format, va_list args) {
  js_builder_add_indent(jsb);
  va_list copy;
  va_copy(copy, args);
  int len = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  if(len > 0) {
    // One more for the NUL that vsnprintf writes after the text.
    char* line = xmalloc(size_add((size_t)len, 1));
    vsnprintf(line, (size_t)len + 1, format, args);
    str_builder_add_str(jsb->text, line);
    free(line);
  }
  str_builder_add_char(jsb->text, '\n');
}

void js_builder_line(JSBuilder* jsb, const char* format, ...) {
  va_list args;
  va_start(args, format);
  add_line(jsb, format, args);
  va_end(args);
}

void js_builder_blank(JSBuilder* jsb) {
  str_builder_add_char(jsb->text, '\n');
}

void js_builder_open(JSBuilder* jsb, const char* format, ...) {
  va_list args;
  va_start(args, format);
  add_line(jsb, format, args);
  va_end(args);
  jsb->depth++;
}

void js_builder_close(JSBuilder* jsb, const char* format, ...) {
  if(jsb->depth > 0) {
    jsb->depth--;
  }
  va_list args;
  va_start(args, format);
  add_line(jsb, format, args);
  va_end(args);
}

void js_builder_add_str(JSBuilder* jsb, const char* str) {
  str_builder_add_str(jsb->text, str);
}

void js_builder_add_format(JSBuilder* jsb, const char* format, ...) {
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  int len = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  if(len > 0) {
    char* text = xmalloc(size_add((size_t)len, 1));
    vsnprintf(text, (size_t)len + 1, format, args);
    str_builder_add_str(jsb->text, text);
    free(text);
  }
  va_end(args);
}

const char* js_builder_peek(const JSBuilder* jsb) {
  return str_builder_peek(jsb->text);
}

char* js_builder_release(JSBuilder* jsb) {
  char* out = str_builder_release(jsb->text, NULL);
  free(jsb);
  return out;
}
