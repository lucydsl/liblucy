#ifndef LUCY_COMPILER_XSTATE_H_
#define LUCY_COMPILER_XSTATE_H_

typedef struct CompileResult {
  bool success;
  char* js;
} CompileResult;

CompileResult* compile_xstate(char*, char*);
char* xs_get_js(CompileResult*);
void destroy_xstate_result(CompileResult*);

#endif