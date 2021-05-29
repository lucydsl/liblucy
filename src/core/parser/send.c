#include "../node.h"
#include "../state.h"
#include "core.h"

int parser_consume_inline_send_args(State* state, void* expr, int _token, char* arg, int argi) {
  SendExpression* send_expression = (SendExpression*)expr;

  if(argi == 0) {
    send_expression->actor = arg;
  } else {
    send_expression->event = arg;
  }

  return 0;
}

int parser_consume_inline_send(State* state, Node* node) {
  int err = 0;

  SendExpression* send_expression = node_create_sendexpression();
  _check(consume_call_expression(state, "send", send_expression, &parser_consume_inline_send_args));

  switch(node->type) {
    case NODE_TRANSITION_TYPE: {
      TransitionNode* transition_node = (TransitionNode*)node;
      TransitionAction* action = node_transition_add_action(transition_node, NULL);
      action->expression = (Expression*)send_expression;
      break;
    }
    case NODE_LOCAL_TYPE: {
      LocalNode* local_node = (LocalNode*)node;
      TransitionAction* action = create_transition_action();
      action->expression = (Expression*)send_expression;
      node_local_add_action(local_node, action);
      break;
    }
  }

  program_add_flag(state->program, PROGRAM_USES_SEND);

  return err;
}