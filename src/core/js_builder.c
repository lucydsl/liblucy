#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "js_builder.h"
#include "str_builder.h"

JSBuilder* js_builder_create() {
  JSBuilder* jsb = malloc(sizeof(*jsb));
  jsb->indent_str = "  ";
  jsb->indent_len = strlen(jsb->indent_str);
  jsb->sb = str_builder_create();
  jsb->ib = str_builder_create();

  return jsb;
}

void js_builder_destroy(JSBuilder* jsb) {
  str_builder_destroy(jsb->sb);
  str_builder_destroy(jsb->ib);
  free(jsb);
}

static bool current_is_newline(JSBuilder* jsb) {
  int len = str_builder_len(jsb->sb);
  char c = str_builder_char_at(jsb->sb, len - 1);
  if(c == '\n') {
    return true;
  }
  return false;
}

void js_builder_add_char(JSBuilder* jsb, char c) {
  str_builder_add_char(jsb->sb, c);
}

void js_builder_add_str(JSBuilder* jsb, char* str) {
  str_builder_add_str(jsb->sb, str, 0);
}

void js_builder_copy_str(JSBuilder* jsb, char* str, size_t start, size_t end) {
  str_builder_copy_str(jsb->sb, str, start, end);
}

void js_builder_safe_key(JSBuilder* jsb, char* str) {
  // TODO make sure this string is a valid key
  js_builder_add_str(jsb, str);
}

void js_builder_add_string(JSBuilder* jsb, char* value) {
  js_builder_add_str(jsb, "'");
  js_builder_add_str(jsb, value);
  js_builder_add_str(jsb, "'");
}

void js_builder_copy_string(JSBuilder* jsb, char* input, size_t start, size_t end) {
  js_builder_add_str(jsb, "'");
  js_builder_copy_str(jsb, input, start, end);
  js_builder_add_str(jsb, "'");
}

void js_builder_add_indent(JSBuilder* jsb) {
  char* indent = str_builder_dump(jsb->ib, NULL);
  str_builder_add_str(jsb->sb, indent, 0);
}

void js_builder_increase_indent(JSBuilder* jsb) {
  str_builder_add_str(jsb->ib, jsb->indent_str, 0);
}

void js_builder_decrease_indent(JSBuilder* jsb) {
  size_t new_size = str_builder_len(jsb->ib) - jsb->indent_len;
  str_builder_truncate(jsb->ib, new_size);
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
  int len = str_builder_len(jsb->sb);
  char c = str_builder_char_at(jsb->sb, len - 2);

  if(c != '{') {
    js_builder_add_str(jsb, ",\n");
  }

  // TODO Quote if necessary
  js_builder_add_indent(jsb);
  js_builder_add_str(jsb, key);
  js_builder_add_str(jsb, ": ");
}

void js_builder_start_prop_no_copy(JSBuilder* jsb, char* key, size_t start, size_t end) {
  int len = str_builder_len(jsb->sb);
  char c = str_builder_char_at(jsb->sb, len - 2);

  if(c != '{') {
    js_builder_add_str(jsb, ",\n");
  }

  // TODO Quote if necessary
  js_builder_add_indent(jsb);
  js_builder_copy_str(jsb, key, start, end);
  js_builder_add_str(jsb, ": ");
}

void js_builder_shorthand_prop(JSBuilder* jsb, char* key) {
  int len = str_builder_len(jsb->sb);
  char c = str_builder_char_at(jsb->sb, len - 2);

  if(c != '{') {
    js_builder_add_str(jsb, ",\n");
  }

  // TODO Quote if necessary
  js_builder_add_indent(jsb);
  js_builder_add_str(jsb, key);
}

void js_builder_start_call(JSBuilder* jsb, char* name) {
  int len = str_builder_len(jsb->sb);
  char c = str_builder_char_at(jsb->sb, len - 1);
  if(c == '\n') {
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
  if(!current_is_newline(jsb)) {
    js_builder_add_str(jsb, "\n");
  }
  js_builder_add_str(jsb, "\nexport ");
}

void js_builder_add_const(JSBuilder* jsb, char* identifier) {
  js_builder_add_str(jsb, "const ");
  js_builder_add_str(jsb, identifier);
}

void js_builder_add_arg(JSBuilder* jsb, char* identifier) {
  int len = str_builder_len(jsb->sb);
  char c = str_builder_char_at(jsb->sb, len - 1);

  // Add a comma if we need to
  if(c != ' ' && c != '(') {
    js_builder_add_str(jsb, ", ");
  }

  js_builder_add_str(jsb, identifier);
}

char* js_builder_dump(JSBuilder* jsb) {
  return str_builder_dump(jsb->sb, NULL);
}