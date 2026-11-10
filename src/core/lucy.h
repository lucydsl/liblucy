#pragma once

/*
 * liblucy's API: compile Lucy source to a module of plain JavaScript state
 * machines and, optionally, TypeScript declarations (.d.ts) for it.
 *
 *   LucyResult* result = lucy_result_create();
 *   lucy_result_set_options(result, true);
 *   lucy_compile(result, source, "machine.lucy");
 *   for(size_t i = 0; i < lucy_result_diagnostic_count(result); i++) {
 *     fputs(lucy_diagnostic_text(lucy_result_diagnostic(result, i)), stderr);
 *   }
 *   if(lucy_result_success(result)) {
 *     puts(lucy_result_js(result));
 *   }
 *   lucy_result_destroy(result);
 *
 * lucyc (src/bin/main.c) uses this, and the wasm module exports these same
 * functions to liblucy.mjs (see WASM_EXPORTS in the Makefile).
 *
 * A result can be used for many compiles, one after another; each compile
 * replaces the last one's output. Everything a result returns belongs to
 * it and is valid until its next compile, lucy_result_set_options or
 * lucy_result_destroy. Running out of memory aborts.
 *
 * The library doesn't print anything. A compile's errors and warnings are
 * diagnostics on the result, for the caller to show.
 */

#include <stdbool.h>
#include <stddef.h>

typedef struct LucyResult LucyResult;

/* An empty result, with the .d.ts option off. Free it with lucy_result_destroy. */
LucyResult* lucy_result_create(void);

/* Free a result and everything it holds. NULL does nothing. */
void lucy_result_destroy(LucyResult* result);

/*
 * Set the options for the next compiles, and clear the result's output.
 * include_dts makes the compile produce a .d.ts as well.
 */
void lucy_result_set_options(LucyResult* result, bool include_dts);

/*
 * Compile source, a NUL-terminated Lucy program read from filename, into
 * result. The source and filename are only read during the call. A NULL
 * source or filename is a failed compile.
 */
void lucy_compile(LucyResult* result, const char* source, const char* filename);

/* Whether the last compile succeeded. */
bool lucy_result_success(const LucyResult* result);

/* The last compile's JavaScript, or NULL if it failed. */
const char* lucy_result_js(const LucyResult* result);

/*
 * The last compile's TypeScript declarations, or NULL if it failed or the
 * include_dts option was off.
 */
const char* lucy_result_dts(const LucyResult* result);

typedef enum LucySeverity {
  // Something in the source is wrong. Most errors fail the compile.
  LUCY_SEVERITY_ERROR,
  // The compile carried on, but something isn't supported.
  LUCY_SEVERITY_WARNING,
} LucySeverity;

/*
 * Where lucyc prints a diagnostic. Errors go to stderr, but a few warnings
 * have always gone to stdout, and lucyc keeps them there so that its output
 * doesn't change.
 */
typedef enum LucyStream {
  LUCY_STREAM_STDERR,
  LUCY_STREAM_STDOUT,
} LucyStream;

typedef struct LucyDiagnostic LucyDiagnostic;

/* How many diagnostics the last compile reported. */
size_t lucy_result_diagnostic_count(const LucyResult* result);

/* The index'th diagnostic, in the order they were reported. */
const LucyDiagnostic* lucy_result_diagnostic(const LucyResult* result, size_t index);

LucySeverity lucy_diagnostic_severity(const LucyDiagnostic* diagnostic);
LucyStream lucy_diagnostic_stream(const LucyDiagnostic* diagnostic);

/* The message alone, such as "Unexpected identifier". */
const char* lucy_diagnostic_message(const LucyDiagnostic* diagnostic);

/*
 * Where in the source an error is, as its text shows it: the line counting
 * from 1, and the column. Both are 0 for a diagnostic without a position
 * (every warning).
 *
 * Known bug: errors reported at the parser's current position (most
 * "Unexpected identifier" errors, for one) point a line too far down.
 */
long lucy_diagnostic_line(const LucyDiagnostic* diagnostic);
long lucy_diagnostic_column(const LucyDiagnostic* diagnostic);

/*
 * The diagnostic as lucyc prints it, with ANSI colours. For an error that
 * is the file name and position, the message, and the source around it.
 */
const char* lucy_diagnostic_text(const LucyDiagnostic* diagnostic);
