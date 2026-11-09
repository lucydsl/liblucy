#include "internal.h"

static int consume_inline_spawn_args(State* state, void* expr, TokenType token, char* arg, int _argi) {
  SpawnExpression* spawn_expression = (SpawnExpression*)expr;
  switch(token) {
    case TOKEN_IDENTIFIER: {
      IdentifierExpression* identifier_expression = node_create_identifierexpression(state->arena);
      identifier_expression->name = arg;
      spawn_expression->target = (Expression*)identifier_expression;
      break;
    }
    case TOKEN_SYMBOL: {
      SymbolExpression* symbol_expression = node_create_symbolexpression(state->arena);
      symbol_expression->name = arg;
      spawn_expression->target = (Expression*)symbol_expression;
      break;
    }
    default: {
      break;
    }
  }
  return 0;
}

int parser_consume_inline_spawn(State* state, AssignExpression* assign_expression) {
  int err = 0;

  SpawnExpression* spawn_expression = node_create_spawnexpression(state->arena);
  _check(consume_call_expression(state, "spawn", spawn_expression, &consume_inline_spawn_args));
  _check(expect_call_argument(state, "spawn", spawn_expression->target, err));
  if(spawn_expression->target == NULL) {
    return err;
  }
  assign_expression->value = (Expression*)spawn_expression;
  program_add_flag(state->program, PROGRAM_USES_SPAWN);

  // The machine this is in. state->current_machine_node was NULL here
  // after a nested machine had ended.
  MachineNode* machine_node = find_closest_machine_node(state->node);
  if(spawn_expression->target->type == EXPRESSION_SYMBOL && machine_node != NULL) {
    machine_node->flags |= MACHINE_USES_SERVICE;
  }

  return err;
}