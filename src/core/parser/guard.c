#include "internal.h"

static ParseStatus consume_inline_guard_args(Parser* parser, void* expr, TokenType token, char* arg, size_t index) {
  (void)index;
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
    return PARSE_ERROR;
  }

  return PARSE_OK;
}

ParseStatus parser_consume_inline_guard(Parser* parser, TransitionNode* transition_node) {
  ParseStatus status = PARSE_OK;
  GuardExpression* guard_expression = expression_create_guard(parser->arena);
  
  if(parse_merge(&status, parser_consume_call(parser, "guard", guard_expression, &consume_inline_guard_args)) == PARSE_FATAL) {
  
    return PARSE_FATAL;
  
  }
  if(parse_merge(&status, parser_expect_call_argument(parser, "guard", guard_expression->ref, status)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }
  if(guard_expression->ref == NULL) {
    return status;
  }

  TransitionGuard* guard = node_transition_add_guard(parser->arena, transition_node, NULL);
  guard->expression = guard_expression;

  MachineNode* parent_machine_node = node_find_machine((Node*)transition_node);
  if(guard->expression->ref->type == EXPRESSION_SYMBOL && parent_machine_node != NULL) {
    parent_machine_node->flags |= MACHINE_USES_GUARD;
  }

  return status;
}

/* Public API */
ParseStatus parser_consume_guard(Parser* parser) {
  AssignmentNode* assignment = node_create_assignment(parser->arena, ASSIGNMENT_GUARD);
  Node* node = (Node*)assignment;
  parser_enter_node(parser, node);

  TokenType token;

  token = parser_next_token(parser);

  if(token != TOKEN_IDENTIFIER) {
    parser_error_unexpected_identifier(parser);
    return PARSE_FATAL;
  }

  assignment->binding_name = parser_take_word(parser);

  token = parser_next_token(parser);

  if(token != TOKEN_ASSIGNMENT) {
    parser_error(parser, "Expected an identifier");
    return PARSE_FATAL;
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
      return PARSE_FATAL;
    }
  }

  parser_add_guard_name(parser, assignment->binding_name);
  parser_leave_node(parser);
  return PARSE_OK;
}
