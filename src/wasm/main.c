#include <stdio.h>

#include "../core/lucy.h"

// liblucy keeps no global state, so there is nothing to set up. main
// still exists because Emscripten runs it when the module is created and
// it is part of the exported API.
int main(void) {
  return 0;
}

// Exported for liblucy.mjs, which calls it after each compile. Printed
// like this, through stdout and stderr, the diagnostics reach Emscripten's
// print and printErr exactly as they did when the compiler printed them
// itself.
void wasm_print_diagnostics(const LucyResult* result);

void wasm_print_diagnostics(const LucyResult* result) {
  size_t count = lucy_result_diagnostic_count(result);
  for(size_t i = 0; i < count; i++) {
    const LucyDiagnostic* diagnostic = lucy_result_diagnostic(result, i);
    FILE* stream = lucy_diagnostic_stream(diagnostic) == LUCY_STREAM_STDOUT ? stdout : stderr;
    fputs(lucy_diagnostic_text(diagnostic), stream);
  }
}
