#pragma once

/*
 * The JavaScript printer's interface to the rest of the compiler. printer.h
 * is shared by the printer's own files.
 */

#include <stdbool.h>

#include "../ast/program.h"
#include "../diagnostic.h"
#include "../support/arena.h"

// What a compile prints.
typedef enum JSFormat {
  // A JavaScript module and its declarations.
  JS_FORMAT_JS,
  // One TypeScript module.
  JS_FORMAT_TS,
} JSFormat;

// Where the views run: the browser's DOM, or a server, as strings.
typedef enum JSTarget {
  JS_TARGET_DOM,
  JS_TARGET_SSR,
} JSTarget;

// The output, which the caller must free(). Only the strings of the
// format asked for are set.
typedef struct JSOutput {
  // The JavaScript module.
  char* js;
  // Its TypeScript declarations.
  char* dts;
  // The TypeScript module.
  char* ts;
} JSOutput;

/*
 * Print program, parsed from source, as a module of plain state machines:
 * JavaScript and its .d.ts, or TypeScript, for target, and for hydration
 * if hydrate is set (see lucy_result_set_hydrate). Working memory comes from arena. Warnings about what the printer can't print are added
 * to diagnostics.
 */
JSOutput js_print(Arena* arena, Program* program, const char* source,
  JSFormat format, JSTarget target, bool hydrate, DiagnosticList* diagnostics);
