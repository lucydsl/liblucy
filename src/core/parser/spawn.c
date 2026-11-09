#include "internal.h"

static int consume_inline_spawn_args(Parser* parser, void* expr, TokenType token, char* arg, int _argi) {
  SpawnExpression* spawn_expression = (SpawnExpression*)expr;
  switch(token) {
    case TOKEN_IDENTIFIER: {
      IdentifierExpression* identifier_expression = expression_create_identifier(parser->arena);
      identifier_expression->name = arg;
      spawn_expression->target = (Expression*)identifier_expression;
      break;
    }
    case TOKEN_SYMBOL: {
      SymbolExpression* symbol_expression = expression_create_symbol(parser->arena);
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

int parser_consume_inline_spawn(Parser* parser, AssignExpression* assign_expression) {
  int err = 0;

  SpawnExpression* spawn_expression = expression_create_spawn(parser->arena);
  _check(parser_consume_call(parser, "spawn", spawn_expression, &consume_inline_spawn_args));
  _check(parser_expect_call_argument(parser, "spawn", spawn_expression->target, err));
  if(spawn_expression->target == NULL) {
    return err;
  }
  assign_expression->value = (Expression*)spawn_expression;
  program_add_flag(parser->program, PROGRAM_USES_SPAWN);

  // The machine this is in. parser->current_machine_node was NULL here
  // after a nested machine had ended.
  MachineNode* machine_node = node_find_machine(parser->node);
  if(spawn_expression->target->type == EXPRESSION_SYMBOL && machine_node != NULL) {
    machine_node->flags |= MACHINE_USES_SERVICE;
  }

  return err;
}