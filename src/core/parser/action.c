#include "internal.h"

#include <stdlib.h>

static ParseStatus consume_inline_action_args(Parser* parser, void* expr, TokenType token, const char* arg, size_t index) {
  (void)index;
  ActionExpression* action_expression = (ActionExpression*)expr;

  if(token == TOKEN_IDENTIFIER) {
    IdentifierExpression* ref = expression_create_identifier(parser->arena);
    ref->name = arg;
    action_expression->ref = (Expression*)ref;
  } else if(token == TOKEN_SYMBOL) {
    SymbolExpression* ref = expression_create_symbol(parser->arena);
    ref->name = arg;
    action_expression->ref = (Expression*)ref;

  } else {
    parser_error_back(parser, parser->token_len, "Unexpected argument to action()");
    return PARSE_ERROR;
  }

  return PARSE_OK;
}

ParseStatus parser_consume_inline_action(Parser* parser, Node* node) {
  ParseStatus status = PARSE_OK;

  ActionExpression* action_expression = expression_create_action(parser->arena);

  if(parse_merge(&status, parser_consume_call(parser, "action", action_expression, &consume_inline_action_args)) == PARSE_FATAL) {

    return PARSE_FATAL;

  }
  if(parse_merge(&status, parser_expect_call_argument(parser, "action", action_expression->ref, status)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }
  if(action_expression->ref == NULL) {
    return status;
  }

  switch(node->type) {
    case NODE_TRANSITION: {
      TransitionNode* transition_node = (TransitionNode*)node;
      TransitionAction* action = node_transition_add_action(parser->arena, transition_node, NULL);
      action->expression = (Expression*)action_expression;
      break;
    }
    case NODE_LOCAL: {
      LocalNode* local_node = (LocalNode*)node;
      TransitionAction* action = node_create_transition_action(parser->arena);
      action->expression = (Expression*)action_expression;
      node_local_add_action(local_node, action);
      break;
    }
    default: {
      break;
    }
  }

  MachineNode* parent_machine_node = node_find_machine(node);
  if(action_expression->ref->type == EXPRESSION_SYMBOL && parent_machine_node != NULL) {
    parent_machine_node->flags |= MACHINE_USES_ACTION;
  }

  return status;
}

ParseStatus parser_consume_action(Parser* parser) {
  ParseStatus status = PARSE_OK;

  AssignmentNode* assignment = node_create_assignment(parser->arena, ASSIGNMENT_ACTION);
  Node *node = (Node*)assignment;
  parser_enter_node(parser, node);

  TokenType token;
  token = parser_next_token(parser);

  if(token != TOKEN_IDENTIFIER) {
    parser_error_unexpected_identifier(parser);
    return PARSE_FATAL;
  }

  const char* binding_name = parser_take_word(parser);
  assignment->binding_name = binding_name;

  token = parser_next_token(parser);
  if(token != TOKEN_ASSIGNMENT) {
    parser_error(parser, "Expected an assignment");
    return PARSE_FATAL;
  }

  token = parser_next_token(parser);
  switch(token) {
    case TOKEN_IDENTIFIER: {
      const char* identifier = parser_take_word(parser);
      Keyword key = keyword_lookup(identifier);
      switch(key) {
        case KEYWORD_ASSIGN: {
          AssignExpression *expression = expression_create_assign(parser->arena);
          // Known bug: an error in the call is reported but ignored, so
          // the compile still succeeds.
          (void)parser_consume_call(parser, "assign", expression, &parser_consume_inline_assign_args);
          assignment->value = (Expression*)expression;
          parser->program->flags |= PROGRAM_USES_ASSIGN;
          break;
        }
        case KEYWORD_SEND: {
          SendExpression* expression = expression_create_send(parser->arena);
          // Known bug: as for assign above.
          (void)parser_consume_call(parser, "send", expression, &parser_consume_inline_send_args);
          assignment->value = (Expression*)expression;
          parser->program->flags |= PROGRAM_USES_SEND;
          break;
        }
        default: {
          IdentifierExpression* expression = expression_create_identifier(parser->arena);
          expression->name = identifier;
          assignment->value = (Expression*)expression;
        }
      }
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

  parser_add_action_name(parser, assignment->binding_name);
  parser_leave_node(parser);
  return status;
}