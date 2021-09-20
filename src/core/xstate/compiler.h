#pragma once

#include <stdbool.h>

typedef struct CompileResult {
  bool success;
  char* js;
  char* dts;
  int flags;
} CompileResult;

CompileResult* xs_create();
void xs_init(CompileResult*, bool, bool);
void compile_xstate(CompileResult*, char*, char*);
char* xs_get_js(CompileResult*);
char* xs_get_dts(CompileResult*);
void destroy_xstate_result(CompileResult*);