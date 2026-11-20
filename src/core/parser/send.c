#include "internal.h"

ParseStatus parser_consume_inline_send_args(Parser* parser, void* expr, TokenType token, const char* arg, size_t index) {
  SendExpression* send_expression = (SendExpression*)expr;

  // index counts the commas too.
  if(index == 0) {
    send_expression->actor = arg;
  } else if(index == 2) {
    send_expression->event = arg;
  } else if(index == 4) {
    // The event's data, a value: send(parent, removed, self)
    send_expression->data = parser_consume_value(parser, token, arg);
    if(send_expression->data == NULL) {
      return PARSE_FATAL;
    }
  } else {
    parser_error_back(parser, parser->token_len, "send takes an actor, an event and, at most, a value for the event's data.");
    return PARSE_FATAL;
  }

  return PARSE_OK;
}

ParseStatus parser_consume_inline_send(Parser* parser, Node* node) {
  ParseStatus status = PARSE_OK;

  SendExpression* send_expression = expression_create_send(parser->arena);
  if(parse_merge(&status, parser_consume_call(parser, "send", send_expression, &parser_consume_inline_send_args)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }

  switch(node->type) {
    case NODE_TRANSITION: {
      TransitionNode* transition_node = (TransitionNode*)node;
      TransitionAction* action = node_transition_add_action(parser->arena, transition_node, NULL);
      action->expression = (Expression*)send_expression;
      break;
    }
    case NODE_LOCAL: {
      LocalNode* local_node = (LocalNode*)node;
      TransitionAction* action = node_create_transition_action(parser->arena);
      action->expression = (Expression*)send_expression;
      node_local_add_action(local_node, action);
      break;
    }
    default: {
      break;
    }
  }

  return status;
}
