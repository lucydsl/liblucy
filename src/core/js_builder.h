#ifndef LUCY_JSBUILDER_H_
#define LUCY_JSBUILDER_H_

#include "str_builder.h"

typedef struct JSBuilder {
  char* indent;
  char* indent_str;
  size_t indent_len;

  str_builder_t* sb;
  str_builder_t* ib;
} JSBuilder;

JSBuilder* js_builder_create();
void js_builder_destroy(JSBuilder*);

void js_builder_add_str(JSBuilder*, char*);
void js_builder_safe_key(JSBuilder*, char*);
void js_builder_add_string(JSBuilder*, char*);

void js_builder_add_indent(JSBuilder*);
void js_builder_increase_indent(JSBuilder*);
void js_builder_decrease_indent(JSBuilder*);
void js_builder_start_object(JSBuilder*);
void js_builder_end_object(JSBuilder*);
void js_builder_start_prop(JSBuilder*, char*);
void js_builder_start_call(JSBuilder*, char*);
void js_builder_end_call(JSBuilder*);
void js_builder_start_array(JSBuilder*, bool);
void js_builder_end_array(JSBuilder*, bool);
void js_builder_add_export(JSBuilder*);
void js_builder_add_const(JSBuilder*, char*);
void js_builder_add_arg(JSBuilder*, char*);

char* js_builder_dump(JSBuilder*);

#endif