#include "program.h"

#include "../support/arena.h"
#include "node.h"

Program* program_create(Arena* arena) {
  return ARENA_NEW(arena, Program);
}
