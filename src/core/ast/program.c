#include "program.h"

#include "../support/arena.h"
#include "node.h"

Program* program_create(Arena* arena) {
  return ARENA_NEW(arena, Program);
}

__attribute__((always_inline)) inline void program_add_flag(Program* program, unsigned flag) {
  program->flags |= flag;
}
