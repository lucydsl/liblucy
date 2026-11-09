#include "node.h"

#include "../support/arena.h"

static Expression* create_expression(Arena* arena, ExpressionType type, size_t size) {
  Expression* expression = arena_alloc(arena, size);
  expression->type = type;
  return expression;
}

AssignExpression* node_create_assignexpression(Arena* arena) {
  return (AssignExpression*)create_expression(arena, EXPRESSION_ASSIGN, sizeof(AssignExpression));
}

IdentifierExpression* node_create_identifierexpression(Arena* arena) {
  return (IdentifierExpression*)create_expression(arena, EXPRESSION_IDENTIFIER, sizeof(IdentifierExpression));
}

GuardExpression* node_create_guardexpression(Arena* arena) {
  return (GuardExpression*)create_expression(arena, EXPRESSION_GUARD, sizeof(GuardExpression));
}

ActionExpression* node_create_actionexpression(Arena* arena) {
  return (ActionExpression*)create_expression(arena, EXPRESSION_ACTION, sizeof(ActionExpression));
}

DelayExpression* node_create_delayexpression(Arena* arena) {
  return (DelayExpression*)create_expression(arena, EXPRESSION_DELAY, sizeof(DelayExpression));
}

OnExpression* node_create_onexpression(Arena* arena) {
  return (OnExpression*)create_expression(arena, EXPRESSION_ON, sizeof(OnExpression));
}

SpawnExpression* node_create_spawnexpression(Arena* arena) {
  return (SpawnExpression*)create_expression(arena, EXPRESSION_SPAWN, sizeof(SpawnExpression));
}

SendExpression* node_create_sendexpression(Arena* arena) {
  return (SendExpression*)create_expression(arena, EXPRESSION_SEND, sizeof(SendExpression));
}

SymbolExpression* node_create_symbolexpression(Arena* arena) {
  return (SymbolExpression*)create_expression(arena, EXPRESSION_SYMBOL, sizeof(SymbolExpression));
}

InvokeExpression* node_create_invokeexpression(Arena* arena) {
  return (InvokeExpression*)create_expression(arena, EXPRESSION_INVOKE, sizeof(InvokeExpression));
}

MemberExpression* node_create_memberexpression(Arena* arena) {
  return (MemberExpression*)create_expression(arena, EXPRESSION_MEMBER, sizeof(MemberExpression));
}
