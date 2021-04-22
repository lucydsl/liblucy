#pragma once

#include "node.h"

#define PROGRAM_USES_ASSIGN 1 << 0
#define PROGRAM_USES_SPAWN 1 << 1
#define PROGRAM_USES_SEND 1 << 2

typedef struct Program {
  Node* body;
  int flags;
} Program;

Program * new_program();
void program_add_flag(Program*, int);