#include "internal.h"

static int consume_inline_guard_args(State* state, void* expr, int token, char* arg, int _argi) {
  GuardExpression* guard_expression = (GuardExpression*)expr;

  if(token == TOKEN_IDENTIFIER) {
    IdentifierExpression* ref = node_create_identifierexpression(state->arena);
    ref->name = arg;
    guard_expression->ref = (Expression*)ref;
  } else if(token == TOKEN_SYMBOL) {
    SymbolExpression* ref = node_create_symbolexpression(state->arena);
    ref->name = arg;
    guard_expression->ref = (Expression*)ref;
  } else {
    error_msg_with_code_block_dec(state, state->token_len, "Unexpected function argument to guard()");
    return 1;
  }

  return 0;
}

int parser_consume_inline_guard(State* state, TransitionNode* transition_node) {
  int err = 0;
  GuardExpression* guard_expression = node_create_guardexpression(state->arena);
  
  _check(consume_call_expression(state, "guard", guard_expression, &consume_inline_guard_args));
  _check(expect_call_argument(state, "guard", guard_expression->ref, err));
  if(guard_expression->ref == NULL) {
    return err;
  }

  TransitionGuard* guard = node_transition_add_guard(state->arena, transition_node, NULL);
  guard->expression = guard_expression;

  MachineNode* parent_machine_node = find_closest_machine_node((Node*)transition_node);
  if(guard->expression->ref->type == EXPRESSION_SYMBOL && parent_machine_node != NULL) {
    parent_machine_node->flags |= MACHINE_USES_GUARD;
  }

  return err;
}

/* Public API */
int parser_consume_guard(State* state) {
  Assignment* assignment = node_create_assignment(state->arena, ASSIGNMENT_GUARD);
  Node* node = (Node*)assignment;
  state_node_set(state, node);

  int token;

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_unexpected_identifier(state, node);
    return 2;
  }

  assignment->binding_name = state_take_word(state);

  token = consume_token(state);

  if(token != TOKEN_ASSIGNMENT) {
    error_msg_with_code_block(state, node, "Expected an identifier");
    return 2;
  }

  token = consume_token(state);
  switch(token) {
    case TOKEN_IDENTIFIER: {
      IdentifierExpression *expression = node_create_identifierexpression(state->arena);
      expression->name = state_take_word(state);
      assignment->value = (Expression*)expression;
      break;
    }
    case TOKEN_SYMBOL: {
      SymbolExpression* expression = node_create_symbolexpression(state->arena);
      expression->name = state_take_word(state);
      assignment->value = (Expression*)expression;
      break;
    }
    default: {
      error_msg_with_code_block_dec(state, state->token_len, "Expected an identifier");
      return 2;
    }
  }

  state_add_guard(state, assignment->binding_name);
  state_node_up(state);
  return 0;
}
