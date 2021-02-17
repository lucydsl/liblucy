#ifndef LUCY_PROGRAM_H_
#define LUCY_PROGRAM_H_

#include "node.h"

typedef struct Program {
  Node* body;
} Program;

Program * new_program();

#endif