#include "js_builder.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../support/mem.h"
#include "../support/str_builder.h"

JSBuilder* js_builder_create(void) {
  JSBuilder* jsb = xmalloc(sizeof(*jsb));
  jsb->indent_unit = "  ";
  jsb->indent_unit_len = strlen(jsb->indent_unit);
  jsb->text = str_builder_create();
  jsb->indent = str_builder_create();

  return jsb;
}

void js_builder_destroy(JSBuilder* jsb) {
  if(jsb == NULL) {
    return;
  }
  str_builder_destroy(jsb->text);
  str_builder_destroy(jsb->indent);
  free(jsb);
}

// The character n places from the end of the text so far (1 is the last),
// or NUL if there aren't that many.
static char char_from_end(JSBuilder* jsb, size_t n) {
  return str_builder_char_from_end(jsb->text, n);
}

// Start a property in an object: a comma and a newline after the property
// before it, if there is one, then the indentation.
static void start_entry(JSBuilder* jsb) {
  // An object starts with "{\n".
  if(char_from_end(jsb, 2) != '{') {
    js_builder_add_str(jsb, ",\n");
  }

  // TODO Quote keys if necessary
  js_builder_add_indent(jsb);
}

void js_builder_add_char(JSBuilder* jsb, char c) {
  str_builder_add_char(jsb->text, c);
}

void js_builder_add_str(JSBuilder* jsb, const char* str) {
  str_builder_add_str(jsb->text, str);
}

void js_builder_copy_str(JSBuilder* jsb, const char* str, size_t start, size_t end) {
  str_builder_copy_str(jsb->text, str, start, end);
}

void js_builder_add_string(JSBuilder* jsb, char* value) {
  js_builder_add_str(jsb, "'");
  js_builder_add_str(jsb, value);
  js_builder_add_str(jsb, "'");
}

void js_builder_copy_string(JSBuilder* jsb, const char* input, size_t start, size_t end) {
  js_builder_add_str(jsb, "'");
  js_builder_copy_str(jsb, input, start, end);
  js_builder_add_str(jsb, "'");
}

void js_builder_add_indent(JSBuilder* jsb) {
  str_builder_add_str(jsb->text, str_builder_peek(jsb->indent));
}

void js_builder_increase_indent(JSBuilder* jsb) {
  str_builder_add_str(jsb->indent, jsb->indent_unit);
}

void js_builder_decrease_indent(JSBuilder* jsb) {
  size_t len = str_builder_len(jsb->indent);
  size_t new_size = len > jsb->indent_unit_len ? len - jsb->indent_unit_len : 0;
  str_builder_truncate(jsb->indent, new_size);
}

void js_builder_start_object(JSBuilder* jsb) {
  js_builder_add_str(jsb, "{\n");
  js_builder_increase_indent(jsb);
}

void js_builder_end_object(JSBuilder* jsb) {
  js_builder_add_str(jsb, "\n");
  js_builder_decrease_indent(jsb);
  js_builder_add_indent(jsb);
  js_builder_add_str(jsb, "}");
}

void js_builder_start_prop(JSBuilder* jsb, char* key) {
  start_entry(jsb);
  js_builder_add_str(jsb, key);
  js_builder_add_str(jsb, ": ");
}

void js_builder_start_prop_no_copy(JSBuilder* jsb, const char* key, size_t start, size_t end) {
  start_entry(jsb);
  js_builder_copy_str(jsb, key, start, end);
  js_builder_add_str(jsb, ": ");
}

void js_builder_shorthand_prop(JSBuilder* jsb, char* key) {
  start_entry(jsb);
  js_builder_add_str(jsb, key);
}

void js_builder_start_call(JSBuilder* jsb, char* name) {
  if(char_from_end(jsb, 1) == '\n') {
    js_builder_add_indent(jsb);
  }

  js_builder_add_str(jsb, name);
  js_builder_add_str(jsb, "(");
}

void js_builder_end_call(JSBuilder* jsb) {
  js_builder_add_str(jsb, ")");
}

void js_builder_start_array(JSBuilder* jsb, bool newline) {
  js_builder_add_str(jsb, "[");
  if(newline) {
    js_builder_add_str(jsb, "\n");
    js_builder_increase_indent(jsb);
  }
}

void js_builder_end_array(JSBuilder* jsb, bool newline) {
  if(newline) {
    js_builder_add_str(jsb, "\n");
    js_builder_decrease_indent(jsb);
    js_builder_add_indent(jsb);
  }
  js_builder_add_str(jsb, "]");
}

void js_builder_add_export(JSBuilder* jsb) {
  if(char_from_end(jsb, 1) != '\n') {
    js_builder_add_str(jsb, "\n");
  }
  js_builder_add_str(jsb, "\nexport ");
}

void js_builder_add_arg(JSBuilder* jsb, char* identifier) {
  char c = char_from_end(jsb, 1);

  // Add a comma if we need to
  if(c != ' ' && c != '(') {
    js_builder_add_str(jsb, ", ");
  }

  js_builder_add_str(jsb, identifier);
}

const char* js_builder_peek(JSBuilder* jsb) {
  return str_builder_peek(jsb->text);
}

char* js_builder_release(JSBuilder* jsb) {
  char* out = str_builder_release(jsb->text, NULL);
  str_builder_destroy(jsb->indent);
  free(jsb);
  return out;
}