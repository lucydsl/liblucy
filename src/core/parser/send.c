#include "internal.h"

int parser_consume_inline_send_args(Parser* parser, void* expr, TokenType _token, char* arg, int argi) {
  SendExpression* send_expression = (SendExpression*)expr;

  if(argi == 0) {
    send_expression->actor = arg;
  } else {
    send_expression->event = arg;
  }

  return 0;
}

int parser_consume_inline_send(Parser* parser, Node* node) {
  int err = 0;

  SendExpression* send_expression = expression_create_send(parser->arena);
  _check(parser_consume_call(parser, "send", send_expression, &parser_consume_inline_send_args));

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

  program_add_flag(parser->program, PROGRAM_USES_SEND);

  return err;
}