#include "../error.h"
#include "../node.h"
#include "../state.h"
#include "core.h"
#include "token.h"
#include "transition.h"

static int consume_invoke_args(State* state, void* expr, int token, char* arg, int _argi) {
  InvokeExpression* invoke_expression = (InvokeExpression*)expr;

  if(token == TOKEN_IDENTIFIER) {
    IdentifierExpression* identifier_expression = node_create_identifierexpression();
    identifier_expression->name = arg;
    invoke_expression->ref = (Expression*)identifier_expression;
  } else if(token == TOKEN_SYMBOL) {
    SymbolExpression* symbol_expression = node_create_symbolexpression();
    symbol_expression->name = arg;
    invoke_expression->ref = (Expression*)symbol_expression;
  } else {
    error_msg_with_code_block_dec(state, state->token_len, "Unexpected argument to invoke()");
    return 1;
  }

  return 0;
}

int parser_consume_invoke(State* state) {
  int err = 0;

  InvokeNode* invoke_node = node_create_invoke();
  Node* node = (Node*)invoke_node;
  state_node_start_pos(state, node, 6); // "invoke"

  Node* parent_node = state->node;
  if(parent_node->type != NODE_STATE_TYPE) {
    error_msg_with_code_block(state, node, "Unexpected parent for invoke.");
    return 2;
  }

  state_node_set(state, node);

  invoke_node->expr = node_create_invokeexpression();
  _check(consume_call_expression(state, "invoke", invoke_node->expr, &consume_invoke_args));
  if(invoke_node->expr->ref->type == EXPRESSION_SYMBOL) {
    MachineNode* machine_node = find_closest_machine_node(parent_node);
    machine_node->flags |= MACHINE_USES_SERVICE;
  }

  int token = consume_token(state);

  if(token != TOKEN_BEGIN_BLOCK) {
    error_unexpected_identifier(state, node);
    return 2;
  }

  while(true) {
    token = consume_token(state);

    switch(token) {
      case TOKEN_EOL: continue;
      case TOKEN_END_BLOCK: {
        goto end;
      }
      case TOKEN_IDENTIFIER: {
        _check(parser_consume_transition(state));
        break;
      }
      default: {
        error_unexpected_identifier(state, node);
        err = 2;
        goto end;
      }
    }
  }

  end: {
    state_node_up(state);
    return err;
  }
}