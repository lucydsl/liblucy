#pragma once

#include <stdbool.h>

/*
 * The result of a compile, and the API that the wasm module exports.
 *
 * Create a result with xs_create, set its options with xs_init, then
 * compile into it with compile_xstate. On success js (and, with the .d.ts
 * option, dts) hold the output: malloc'd strings that the result owns.
 * destroy_xstate_result frees the result and its strings.
 *
 * A result can be used for more than one compile. xs_init and
 * compile_xstate free whatever output it held before.
 *
 * success is the first field because liblucy.mjs reads it as the first
 * byte of the struct.
 */
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
