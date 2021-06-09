#pragma once

typedef struct CompileResult {
  bool success;
  char* js;
  int flags;
} CompileResult;

CompileResult* xs_create();
void xs_init(CompileResult*, int);
void compile_xstate(CompileResult*, char*, char*);
char* xs_get_js(CompileResult*);
void destroy_xstate_result(CompileResult*);