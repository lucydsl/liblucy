#pragma once

/*
 * liblucy's API: compile Lucy source to an XState JavaScript module and,
 * optionally, TypeScript declarations (.d.ts) for it.
 *
 *   LucyResult* result = lucy_result_create();
 *   lucy_result_set_options(result, false, true);
 *   lucy_compile_xstate(result, source, "machine.lucy");
 *   if(lucy_result_success(result)) {
 *     puts(lucy_result_js(result));
 *   }
 *   lucy_result_destroy(result);
 *
 * lucyc (src/bin/main.c) uses this, and the wasm module exports these same
 * functions to liblucy.mjs (see EXPORTED_FUNCTIONS in the Makefile).
 *
 * A result can be used for many compiles, one after another; each compile
 * replaces the last one's output. Everything a result returns belongs to
 * it and is valid until its next compile, lucy_result_set_options or
 * lucy_result_destroy. Running out of memory aborts.
 */

#include <stdbool.h>

typedef struct LucyResult LucyResult;

/* An empty result, with both options off. Free it with lucy_result_destroy. */
LucyResult* lucy_result_create(void);

/* Free a result and everything it holds. NULL does nothing. */
void lucy_result_destroy(LucyResult* result);

/*
 * Set the options for the next compiles, and clear the result's output.
 * use_remote_imports imports XState from a CDN URL instead of "xstate";
 * include_dts makes the compile produce a .d.ts as well.
 */
void lucy_result_set_options(LucyResult* result, bool use_remote_imports, bool include_dts);

/*
 * Compile source, a NUL-terminated Lucy program read from filename, into
 * result. The source and filename are only read during the call. A NULL
 * source or filename is a failed compile.
 */
void lucy_compile_xstate(LucyResult* result, const char* source, const char* filename);

/* Whether the last compile succeeded. */
bool lucy_result_success(const LucyResult* result);

/* The last compile's JavaScript, or NULL if it failed. */
const char* lucy_result_js(const LucyResult* result);

/*
 * The last compile's TypeScript declarations, or NULL if it failed or the
 * include_dts option was off.
 */
const char* lucy_result_dts(const LucyResult* result);
