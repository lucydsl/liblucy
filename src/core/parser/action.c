#include <stdlib.h>
#include "../error.h"
#include "../keyword.h"
#include "../node.h"
#include "../state.h"
#include "assign.h"
#include "core.h"
#include "send.h"
#include "token.h"

static int consume_inline_action_args(State* state, void* expr, int token, char* arg, int _argi) {
  ActionExpression* action_expression = (ActionExpression*)expr;

  if(token == TOKEN_IDENTIFIER) {
    IdentifierExpression* ref = node_create_identifierexpression();
    ref->name = arg;
    action_expression->ref = (Expression*)ref;
  } else if(token == TOKEN_SYMBOL) {
    SymbolExpression* ref = node_create_symbolexpression();
    ref->name = arg;
    action_expression->ref = (Expression*)ref;

  } else {
    error_msg_with_code_block_dec(state, state->token_len, "Unexpected argument to action()");
    return 1;
  }

  return 0;
}

int parser_consume_inline_action(State* state, Node* node) {
  int err = 0;

  ActionExpression* action_expression = node_create_actionexpression();

  _check(consume_call_expression(state, "action", action_expression, &consume_inline_action_args));

  switch(node->type) {
    case NODE_TRANSITION_TYPE: {
      TransitionNode* transition_node = (TransitionNode*)node;
      TransitionAction* action = node_transition_add_action(transition_node, NULL);
      action->expression = (Expression*)action_expression;

      if(action_expression->ref->type == EXPRESSION_SYMBOL) {
        MachineNode* parent_machine_node = find_closest_machine_node((Node*)transition_node);
        parent_machine_node->flags |= MACHINE_USES_ACTION;
      }
      break;
    }
    case NODE_LOCAL_TYPE: {
      LocalNode* local_node = (LocalNode*)node;
      TransitionAction* action = create_transition_action();
      action->expression = (Expression*)action_expression;
      node_local_add_action(local_node, action);
      break;
    }
  }

  if(action_expression->ref->type == EXPRESSION_SYMBOL) {
    MachineNode* parent_machine_node = find_closest_machine_node(node);
    parent_machine_node->flags |= MACHINE_USES_ACTION;
  }

  return err;
}

int parser_consume_action(State* state) {
  int err = 0;

  Assignment* assignment = node_create_assignment(ASSIGNMENT_ACTION);
  Node *node = (Node*)assignment;
  state_node_set(state, node);

  int token;
  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_unexpected_identifier(state, node);
    return 2;
  }

  char* binding_name = state_take_word(state);
  assignment->binding_name = binding_name;

  token = consume_token(state);
  if(token != TOKEN_ASSIGNMENT) {
    error_msg_with_code_block(state, node, "Expected an assignment");
    return 2;
  }

  token = consume_token(state);
  switch(token) {
    case TOKEN_IDENTIFIER: {
      char* identifier = state_take_word(state);
      unsigned short key = keyword_get(identifier);
      switch(key) {
        case KW_ASSIGN: {
          AssignExpression *expression = node_create_assignexpression();
          consume_call_expression(state, "assign", expression, &parser_consume_inline_assign_args);
          assignment->value = (Expression*)expression;
          program_add_flag(state->program, PROGRAM_USES_ASSIGN);
          free(identifier);
          break;
        }
        case KW_SEND: {
          SendExpression* expression = node_create_sendexpression();
          consume_call_expression(state, "send", expression, &parser_consume_inline_send_args);
          assignment->value = (Expression*)expression;
          program_add_flag(state->program, PROGRAM_USES_SEND);
          free(identifier);
          break;
        }
        default: {
          IdentifierExpression* expression = node_create_identifierexpression();
          expression->name = identifier;
          assignment->value = (Expression*)expression;
        }
      }
      break;
    }
    case TOKEN_SYMBOL: {
      SymbolExpression* expression = node_create_symbolexpression();
      expression->name = state_take_word(state);
      assignment->value = (Expression*)expression;
      break;
    }
    default: {
      error_msg_with_code_block_dec(state, state->token_len, "Expected an identifier");
      return 2;
    }
  }

  end: {
    state_add_action(state, assignment->binding_name);
    state_node_up(state);
    return err;
  }
}