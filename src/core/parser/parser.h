#pragma once

#include <stdbool.h>

#include "../ast/program.h"
#include "../diagnostic.h"
#include "../support/arena.h"

typedef struct ParseResult {
  bool success;
  Program* program;
} ParseResult;

/*
 * Parse source, read from filename. The program and all of its nodes are
 * allocated from arena and are valid until it is destroyed, whether or not
 * the parse succeeded. Errors are added to diagnostics.
 */
ParseResult parse(Arena* arena, char* source, char* filename, DiagnosticList* diagnostics);