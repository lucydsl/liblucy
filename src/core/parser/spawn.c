#include "../node.h"
#include "../state.h"
#include "core.h"
#include "token.h"

static int consume_inline_spawn_args(State* state, void* expr, int token, char* arg, int _argi) {
  SpawnExpression* spawn_expression = (SpawnExpression*)expr;
  switch(token) {
    case TOKEN_IDENTIFIER: {
      IdentifierExpression* identifier_expression = node_create_identifierexpression();
      identifier_expression->name = arg;
      spawn_expression->target = (Expression*)identifier_expression;
      break;
    }
    case TOKEN_SYMBOL: {
      SymbolExpression* symbol_expression = node_create_symbolexpression();
      symbol_expression->name = arg;
      spawn_expression->target = (Expression*)symbol_expression;
      break;
    }
  }
  return 0;
}

int parser_consume_inline_spawn(State* state, AssignExpression* assign_expression) {
  int err = 0;

  SpawnExpression* spawn_expression = node_create_spawnexpression();
  _check(consume_call_expression(state, "spawn", spawn_expression, &consume_inline_spawn_args));
  assign_expression->value = (Expression*)spawn_expression;
  program_add_flag(state->program, PROGRAM_USES_SPAWN);

  if(spawn_expression->target->type == EXPRESSION_SYMBOL) {
    // TODO we should be telling the machine that we need services but this is kind of hard at the moment.
    // I think what should be happening is that an Expression should be a Node.
  }

  return err;
}