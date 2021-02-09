#include <stdlib.h>
#include "program.h"
#include "node.h"

Program * new_program()
{
  Program * program = malloc(sizeof(Program));
  return program;
}
