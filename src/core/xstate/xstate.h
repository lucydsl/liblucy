#pragma once

/*
 * The XState printer's interface to the rest of the compiler. printer.h is
 * shared by the printer's own files.
 */

#include <stdbool.h>

#include "../ast/program.h"
#include "../diagnostic.h"
#include "../support/arena.h"

typedef struct XStateOutput {
  // The JavaScript module. The caller must free() it.
  char* js;
  // Its TypeScript declarations, if they were asked for, else NULL. The
  // caller must free() it.
  char* dts;
} XStateOutput;

/*
 * Print program, parsed from source, as an XState module. use_remote_imports
 * imports XState from a CDN rather than "xstate", and include_dts prints
 * the .d.ts as well. Working memory comes from arena. Warnings about what
 * the printer can't print are added to diagnostics.
 */
XStateOutput xstate_print(Arena* arena, Program* program, const char* source,
  bool use_remote_imports, bool include_dts, DiagnosticList* diagnostics);
