#include "node.h"

#include "../support/arena.h"

static Expression* create_expression(Arena* arena, ExpressionType type, size_t size) {
  Expression* expression = arena_alloc(arena, size);
  expression->type = type;
  return expression;
}

AssignExpression* expression_create_assign(Arena* arena) {
  return (AssignExpression*)create_expression(arena, EXPRESSION_ASSIGN, sizeof(AssignExpression));
}

IdentifierExpression* expression_create_identifier(Arena* arena) {
  return (IdentifierExpression*)create_expression(arena, EXPRESSION_IDENTIFIER, sizeof(IdentifierExpression));
}

GuardExpression* expression_create_guard(Arena* arena) {
  return (GuardExpression*)create_expression(arena, EXPRESSION_GUARD, sizeof(GuardExpression));
}

ActionExpression* expression_create_action(Arena* arena) {
  return (ActionExpression*)create_expression(arena, EXPRESSION_ACTION, sizeof(ActionExpression));
}

DelayExpression* expression_create_delay(Arena* arena) {
  return (DelayExpression*)create_expression(arena, EXPRESSION_DELAY, sizeof(DelayExpression));
}

OnExpression* expression_create_on(Arena* arena) {
  return (OnExpression*)create_expression(arena, EXPRESSION_ON, sizeof(OnExpression));
}

SpawnExpression* expression_create_spawn(Arena* arena) {
  return (SpawnExpression*)create_expression(arena, EXPRESSION_SPAWN, sizeof(SpawnExpression));
}

SendExpression* expression_create_send(Arena* arena) {
  return (SendExpression*)create_expression(arena, EXPRESSION_SEND, sizeof(SendExpression));
}

SymbolExpression* expression_create_symbol(Arena* arena) {
  return (SymbolExpression*)create_expression(arena, EXPRESSION_SYMBOL, sizeof(SymbolExpression));
}

InvokeExpression* expression_create_invoke(Arena* arena) {
  return (InvokeExpression*)create_expression(arena, EXPRESSION_INVOKE, sizeof(InvokeExpression));
}

MemberExpression* expression_create_member(Arena* arena) {
  return (MemberExpression*)create_expression(arena, EXPRESSION_MEMBER, sizeof(MemberExpression));
}
