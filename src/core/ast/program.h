#pragma once

#include "../support/arena.h"
#include "node.h"

// Program.flags: which XState functions the output imports.
#define PROGRAM_USES_ASSIGN (1u << 0)
#define PROGRAM_USES_SPAWN (1u << 1)
#define PROGRAM_USES_SEND (1u << 2)

typedef struct Program {
  Node* body;
  unsigned flags;
} Program;

/* The program is allocated from arena, like its nodes. */
Program* program_create(Arena*);
void program_add_flag(Program*, unsigned);