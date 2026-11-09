#pragma once

#include "arena.h"
#include "node.h"

#define PROGRAM_USES_ASSIGN 1 << 0
#define PROGRAM_USES_SPAWN 1 << 1
#define PROGRAM_USES_SEND 1 << 2

typedef struct Program {
  Node* body;
  int flags;
} Program;

/* The program is allocated from arena, like its nodes. */
Program* new_program(Arena*);
void program_add_flag(Program*, int);