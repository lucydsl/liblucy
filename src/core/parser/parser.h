#pragma once

#include <stdbool.h>
#include "../program.h"

typedef struct ParseResult {
  bool success;
  Program* program;
} ParseResult;

ParseResult* parse(char*, char*);
void parser_init();