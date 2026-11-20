#include "internal.h"

/*
 * remove(key, value): the key of a list of machines (one that append adds
 * to), and the machine to stop and take out of it. The value is event.data
 * or context.key: a function isn't allowed, as the output uses the value
 * more than once.
 */

static const char* const remove_help =
  "remove takes the key of a list and the machine to remove, event.data or context.name, as in remove(todos, event.data).";

static ParseStatus consume_remove_args(Parser* parser, void* expr, TokenType token, const char* arg, size_t index) {
  RemoveExpression* remove_expression = (RemoveExpression*)expr;

  // index counts the commas too.
  if(index == 0 && token == TOKEN_IDENTIFIER) {
    remove_expression->key = arg;
  } else if(index == 2 && token == TOKEN_MEMBER_EXPRESSION) {
    remove_expression->value = parser_consume_value(parser, token, arg);
    if(remove_expression->value == NULL) {
      return PARSE_FATAL;
    }
  } else {
    parser_error_back(parser, parser->token_len, remove_help);
    return PARSE_FATAL;
  }
  return PARSE_OK;
}

ParseStatus parser_consume_remove_call(Parser* parser, RemoveExpression* expression) {
  ParseStatus status = PARSE_OK;
  if(parse_merge(&status, parser_consume_call(parser, "remove", expression, &consume_remove_args)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }
  if(status == PARSE_OK && (expression->key == NULL || expression->value == NULL)) {
    parser_error_back(parser, parser->token_len, remove_help);
    return PARSE_FATAL;
  }
  return status;
}

ParseStatus parser_consume_inline_remove(Parser* parser, Node* node) {
  ParseStatus status = PARSE_OK;

  RemoveExpression* remove_expression = expression_create_remove(parser->arena);
  if(parse_merge(&status, parser_consume_remove_call(parser, remove_expression)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }

  switch(node->type) {
    case NODE_TRANSITION: {
      TransitionNode* transition_node = (TransitionNode*)node;
      TransitionAction* action = node_transition_add_action(parser->arena, transition_node, NULL);
      action->expression = (Expression*)remove_expression;
      break;
    }
    case NODE_LOCAL: {
      LocalNode* local_node = (LocalNode*)node;
      TransitionAction* action = node_create_transition_action(parser->arena);
      action->expression = (Expression*)remove_expression;
      node_local_add_action(local_node, action);
      break;
    }
    default: {
      break;
    }
  }

  return status;
}
