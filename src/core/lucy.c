#include "lucy.h"

#include <stdbool.h>
#include <stdlib.h>

#include "diagnostic.h"
#include "parser/parser.h"
#include "support/arena.h"
#include "support/mem.h"
#include "xstate/xstate.h"

struct LucyResult {
  bool use_remote_imports;
  bool include_dts;

  // The last compile's output, malloc'd and owned by the result.
  bool success;
  char* js;
  char* dts;
  DiagnosticList diagnostics;
};

// Free the output of the last compile, so the result can be used again.
static void clear_output(LucyResult* result) {
  free(result->js);
  free(result->dts);
  result->success = false;
  result->js = NULL;
  result->dts = NULL;
  diagnostic_list_clear(&result->diagnostics);
}

LucyResult* lucy_result_create(void) {
  // Zeroed: no output, and both options off.
  LucyResult* result = xcalloc(1, sizeof(LucyResult));
  diagnostic_list_init(&result->diagnostics);
  return result;
}

void lucy_result_destroy(LucyResult* result) {
  if(result == NULL) {
    return;
  }
  clear_output(result);
  free(result);
}

void lucy_result_set_options(LucyResult* result, bool use_remote_imports, bool include_dts) {
  clear_output(result);
  result->use_remote_imports = use_remote_imports;
  result->include_dts = include_dts;
}

void lucy_compile_xstate(LucyResult* result, const char* source, const char* filename) {
  // Output from an earlier compile into the same result is replaced.
  clear_output(result);

  if(source == NULL || filename == NULL) {
    return;
  }

  // Everything this compile allocates, apart from the strings it returns
  // in result, comes from this arena and is freed with it at the end.
  Arena arena;
  arena_init(&arena);

  ParseResult parsed = parse(&arena, source, filename,
    &result->diagnostics);
  if(parsed.success) {
    XStateOutput output = xstate_print(&arena, parsed.program, source,
      result->use_remote_imports, result->include_dts, &result->diagnostics);
    result->success = true;
    result->js = output.js;
    result->dts = output.dts;
  }

  arena_destroy(&arena);
}

bool lucy_result_success(const LucyResult* result) {
  return result->success;
}

const char* lucy_result_js(const LucyResult* result) {
  return result->js;
}

const char* lucy_result_dts(const LucyResult* result) {
  return result->dts;
}

size_t lucy_result_diagnostic_count(const LucyResult* result) {
  return result->diagnostics.count;
}

const LucyDiagnostic* lucy_result_diagnostic(const LucyResult* result, size_t index) {
  if(index >= result->diagnostics.count) {
    return NULL;
  }
  return &result->diagnostics.items[index];
}

LucySeverity lucy_diagnostic_severity(const LucyDiagnostic* diagnostic) {
  return diagnostic->severity;
}

LucyStream lucy_diagnostic_stream(const LucyDiagnostic* diagnostic) {
  return diagnostic->stream;
}

const char* lucy_diagnostic_message(const LucyDiagnostic* diagnostic) {
  return diagnostic->message;
}

long lucy_diagnostic_line(const LucyDiagnostic* diagnostic) {
  return diagnostic->line;
}

long lucy_diagnostic_column(const LucyDiagnostic* diagnostic) {
  return diagnostic->column;
}

const char* lucy_diagnostic_text(const LucyDiagnostic* diagnostic) {
  return diagnostic->text;
}
