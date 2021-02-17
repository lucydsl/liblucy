#ifndef LUCY_COMPILER_XSTATE_H_
#define LUCY_COMPILER_XSTATE_H_

typedef struct CompileResult {
  bool success;
  char* error;
  char* js;
} CompileResult;

CompileResult* compile_xstate(char*);
char* xs_get_js(CompileResult*);

#endif