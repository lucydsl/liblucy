#pragma once

#include <stdbool.h>
#include "../arena.h"
#include "../program.h"

typedef struct ParseResult {
  bool success;
  Program* program;
} ParseResult;

/*
 * Parse source. The program and all of its nodes are allocated from
 * arena and are valid until it is destroyed, whether or not the parse
 * succeeded.
 */
ParseResult parse(Arena* arena, char* source, char* filename);