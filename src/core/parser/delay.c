#include "internal.h"

#include <stdlib.h>
#include <string.h>

static int consume_inline_delay_args(State* state, void* expr, TokenType token, char* arg, int _argi) {
  DelayExpression* delay_expression = (DelayExpression*)expr;

  int time = 0;
  Expression* ref = NULL;

  switch(token) {
    case TOKEN_INTEGER: {
      Timeframe tf = timeframe_parse(arg, strlen(arg));

      if(tf.error != NULL) {
        error_msg_with_code_block(state, NULL, tf.error);
        return 2;
      }

      time = tf.time;
      break;
    }
    case TOKEN_TIMEFRAME: {
      Timeframe tf = timeframe_parse(arg, state->word_len);

      if(tf.error != NULL) {
        error_msg_with_code_block(state, NULL, tf.error);
        return 2;
      }

      time = tf.time;
      break;
    }
    case TOKEN_IDENTIFIER: {
      IdentifierExpression* identifier_expression = node_create_identifierexpression(state->arena);
      identifier_expression->name = arg;
      ref = (Expression*)identifier_expression;
      break;
    }
    case TOKEN_SYMBOL: {
      SymbolExpression* symbol_expression = node_create_symbolexpression(state->arena);
      symbol_expression->name = arg;
      ref = (Expression*)symbol_expression;
      break;
    }
    default: {
      error_msg_with_code_block(state, NULL, "Expected either an integer time (in milliseconds) or a timeframe such as 200ms.");
      return 2;
    }
  }

  delay_expression->time = time;
  delay_expression->ref = ref;

  return 0;
}

int parser_consume_inline_delay(State* state, TransitionNode* transition_node, StateNode* state_node) {
  int err = 0;

  transition_node->type = TRANSITION_DELAY;
  DelayExpression* expression = node_create_delayexpression(state->arena);

  _check(consume_call_expression(state, "delay", expression, &consume_inline_delay_args));

  node_transition_add_delay(state->arena, transition_node, NULL, expression);
  if(expression-> ref != NULL && expression->ref->type == EXPRESSION_SYMBOL) {
    MachineNode* machine_node = find_closest_machine_node((Node*)state_node);
    machine_node->flags |= MACHINE_USES_DELAY;
  }

  return err;
}