#pragma once

/*
 * The JavaScript printer's interface to the rest of the compiler. printer.h
 * is shared by the printer's own files.
 */

#include <stdbool.h>

#include "../ast/program.h"
#include "../diagnostic.h"
#include "../support/arena.h"

typedef struct JSOutput {
  // The JavaScript module. The caller must free() it.
  char* js;
  // Its TypeScript declarations, if they were asked for, else NULL. The
  // caller must free() it.
  char* dts;
} JSOutput;

/*
 * Print program, parsed from source, as a module of plain JavaScript state
 * machines, and its .d.ts as well if include_dts is set. Working memory
 * comes from arena. Warnings about what the printer can't print are added
 * to diagnostics.
 */
JSOutput js_print(Arena* arena, Program* program, const char* source,
  bool include_dts, DiagnosticList* diagnostics);
