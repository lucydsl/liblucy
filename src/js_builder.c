#include <stdio.h> // todo remove
#include <stdlib.h>
#include <string.h>
#include "js_builder.h"
#include "str_builder.h"

JSBuilder* js_builder_create()
{
  JSBuilder* jsb = malloc(sizeof(JSBuilder));
  jsb->indent_str = "  ";
  jsb->indent_len = strlen(jsb->indent_str);
  jsb->sb = str_builder_create();

  jsb->ib = str_builder_create();
  str_builder_add_str(jsb->ib, jsb->indent_str, 0);

  return jsb;
}

void js_builder_add_str(JSBuilder* jsb, char* str)
{
  str_builder_add_str(jsb->sb, str, 0);
}

void js_builder_safe_key(JSBuilder* jsb, char* str)
{
  // TODO make sure this string is a valid key
  js_builder_add_str(jsb, str);
}

void js_builder_add_string(JSBuilder* jsb, char* value)
{
  js_builder_add_str(jsb, "'");
  js_builder_add_str(jsb, value);
  js_builder_add_str(jsb, "'");
}

void js_builder_add_indent(JSBuilder* jsb)
{
  char* indent = str_builder_dump(jsb->ib, NULL);
  str_builder_add_str(jsb->sb, indent, 0);
}

void js_builder_increase_indent(JSBuilder* jsb)
{
  str_builder_add_str(jsb->ib, jsb->indent_str, 0);
}

void js_builder_decrease_indent(JSBuilder* jsb)
{
  size_t new_size = str_builder_len(jsb->ib) - jsb->indent_len;
  str_builder_truncate(jsb->ib, new_size);
}

char* js_builder_dump(JSBuilder* jsb)
{
  return str_builder_dump(jsb->sb, NULL);
}