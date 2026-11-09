#include "arena.h"
#include "program.h"
#include "node.h"

Program* new_program(Arena* arena) {
  return ARENA_NEW(arena, Program);
}

__attribute__((always_inline)) inline void program_add_flag(Program* program, int flag) {
  program->flags |= flag;
}
