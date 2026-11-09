#include "internal.h"

static int consume_inline_guard_args(Parser* parser, void* expr, TokenType token, char* arg, int _argi) {
  GuardExpression* guard_expression = (GuardExpression*)expr;

  if(token == TOKEN_IDENTIFIER) {
    IdentifierExpression* ref = expression_create_identifier(parser->arena);
    ref->name = arg;
    guard_expression->ref = (Expression*)ref;
  } else if(token == TOKEN_SYMBOL) {
    SymbolExpression* ref = expression_create_symbol(parser->arena);
    ref->name = arg;
    guard_expression->ref = (Expression*)ref;
  } else {
    parser_error_back(parser, parser->token_len, "Unexpected function argument to guard()");
    return 1;
  }

  return 0;
}

int parser_consume_inline_guard(Parser* parser, TransitionNode* transition_node) {
  int err = 0;
  GuardExpression* guard_expression = expression_create_guard(parser->arena);
  
  _check(parser_consume_call(parser, "guard", guard_expression, &consume_inline_guard_args));
  _check(parser_expect_call_argument(parser, "guard", guard_expression->ref, err));
  if(guard_expression->ref == NULL) {
    return err;
  }

  TransitionGuard* guard = node_transition_add_guard(parser->arena, transition_node, NULL);
  guard->expression = guard_expression;

  MachineNode* parent_machine_node = node_find_machine((Node*)transition_node);
  if(guard->expression->ref->type == EXPRESSION_SYMBOL && parent_machine_node != NULL) {
    parent_machine_node->flags |= MACHINE_USES_GUARD;
  }

  return err;
}

/* Public API */
int parser_consume_guard(Parser* parser) {
  AssignmentNode* assignment = node_create_assignment(parser->arena, ASSIGNMENT_GUARD);
  Node* node = (Node*)assignment;
  parser_enter_node(parser, node);

  TokenType token;

  token = parser_next_token(parser);

  if(token != TOKEN_IDENTIFIER) {
    parser_error_unexpected_identifier(parser, node);
    return 2;
  }

  assignment->binding_name = parser_take_word(parser);

  token = parser_next_token(parser);

  if(token != TOKEN_ASSIGNMENT) {
    parser_error(parser, node, "Expected an identifier");
    return 2;
  }

  token = parser_next_token(parser);
  switch(token) {
    case TOKEN_IDENTIFIER: {
      IdentifierExpression *expression = expression_create_identifier(parser->arena);
      expression->name = parser_take_word(parser);
      assignment->value = (Expression*)expression;
      break;
    }
    case TOKEN_SYMBOL: {
      SymbolExpression* expression = expression_create_symbol(parser->arena);
      expression->name = parser_take_word(parser);
      assignment->value = (Expression*)expression;
      break;
    }
    default: {
      parser_error_back(parser, parser->token_len, "Expected an identifier");
      return 2;
    }
  }

  parser_add_guard_name(parser, assignment->binding_name);
  parser_leave_node(parser);
  return 0;
}
