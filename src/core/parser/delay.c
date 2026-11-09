#include "internal.h"

#include <stdlib.h>
#include <string.h>

static int consume_inline_delay_args(Parser* parser, void* expr, TokenType token, char* arg, int _argi) {
  DelayExpression* delay_expression = (DelayExpression*)expr;

  int time = 0;
  Expression* ref = NULL;

  switch(token) {
    case TOKEN_INTEGER: {
      Timeframe tf = timeframe_parse(arg, strlen(arg));

      if(tf.error != NULL) {
        parser_error(parser, NULL, tf.error);
        return 2;
      }

      time = tf.time;
      break;
    }
    case TOKEN_TIMEFRAME: {
      Timeframe tf = timeframe_parse(arg, parser->word_len);

      if(tf.error != NULL) {
        parser_error(parser, NULL, tf.error);
        return 2;
      }

      time = tf.time;
      break;
    }
    case TOKEN_IDENTIFIER: {
      IdentifierExpression* identifier_expression = expression_create_identifier(parser->arena);
      identifier_expression->name = arg;
      ref = (Expression*)identifier_expression;
      break;
    }
    case TOKEN_SYMBOL: {
      SymbolExpression* symbol_expression = expression_create_symbol(parser->arena);
      symbol_expression->name = arg;
      ref = (Expression*)symbol_expression;
      break;
    }
    default: {
      parser_error(parser, NULL, "Expected either an integer time (in milliseconds) or a timeframe such as 200ms.");
      return 2;
    }
  }

  delay_expression->time = time;
  delay_expression->ref = ref;

  return 0;
}

int parser_consume_inline_delay(Parser* parser, TransitionNode* transition_node, StateNode* state_node) {
  int err = 0;

  transition_node->type = TRANSITION_DELAY;
  DelayExpression* expression = expression_create_delay(parser->arena);

  _check(parser_consume_call(parser, "delay", expression, &consume_inline_delay_args));

  node_transition_set_delay(parser->arena, transition_node, NULL, expression);
  if(expression-> ref != NULL && expression->ref->type == EXPRESSION_SYMBOL) {
    MachineNode* machine_node = node_find_machine((Node*)state_node);
    machine_node->flags |= MACHINE_USES_DELAY;
  }

  return err;
}