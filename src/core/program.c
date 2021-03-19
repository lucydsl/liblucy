#include <stdlib.h>
#include "program.h"
#include "node.h"

Program * new_program() {
  Program * program = malloc(sizeof(Program));
  program->body = NULL;
  program->flags = 0;
  return program;
}

__attribute__((always_inline)) inline void program_add_flag(Program* program, int flag) {
  program->flags |= flag;
}
