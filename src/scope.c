#include "scope.h"

Scope* scope_create_scope(Scope* parent)
{
  Scope *scope = malloc(sizeof *scope);
  scope->parent = parent;
  return scope;
}