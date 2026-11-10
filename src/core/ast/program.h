#pragma once

#include "../support/arena.h"
#include "node.h"

typedef struct Program {
  Node* body;
} Program;

/* The program is allocated from arena, like its nodes. */
Program* program_create(Arena*);
