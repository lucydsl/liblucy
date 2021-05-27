#include <stdlib.h>
#include <string.h>
#include "../error.h"
#include "../keyword.h"
#include "../node.h"
#include "../state.h"
#include "action.h"
#include "assign.h"
#include "core.h"
#include "delay.h"
#include "guard.h"
#include "parser.h"
#include "send.h"
#include "token.h"

static char* get_event_name(TransitionNode* transition_node) {
  char* name = NULL;
  Expression* event_expression = transition_node->event;
  if(event_expression->type == EXPRESSION_IDENTIFIER) {
    IdentifierExpression* idexpr = (IdentifierExpression*)event_expression;
    name = idexpr->name;
  } else if(event_expression->type == EXPRESSION_ON) {
    OnExpression* onexpr = (OnExpression*)event_expression;
    name = onexpr->name;
  }
  return name;
}

static void link_event_transition(StateNode* state_node, TransitionNode* transition_node) {
  char* event_name = get_event_name(transition_node);
  TransitionNode* child_transition_node = state_node->event_transition;
  if(child_transition_node == NULL) {
    state_node->event_transition = transition_node;
  } else {
    do {
      char* name = get_event_name(child_transition_node);
      if(strcmp(name, event_name) == 0) {
        TransitionNode* linked = child_transition_node;
        while(linked->link != NULL) {
          linked = linked->link;
        }
        linked->link = transition_node;
      }

      if(child_transition_node->next == NULL) {
        child_transition_node->next = transition_node;
        break;
      } else {
        child_transition_node = child_transition_node->next;
      }
    } while(true);
  }
}


static int consume_on_args(State* state, void* expr, int _token, char* arg, int _argi) {
  OnExpression* on_expression = (OnExpression*)expr;
  on_expression->name = arg;
  return 0;
}

static int consume_on(State* state, TransitionNode* transition_node) {
  int err = 0;

  OnExpression* expression = node_create_onexpression();
  _check(consume_call_expression(state, "on", expression, &consume_on_args));
  transition_node->event = (Expression*)expression;

  return err;
}

/* Public API */
int parser_consume_transition(State* state) {
  int err = 0;
  TransitionNode* transition_node = node_create_transition();

  Node* current_node = state->node;
  size_t current_node_type = current_node->type;
  Node* state_node_node = find_closest_node_of_type(current_node, NODE_STATE_TYPE);
  StateNode* state_node = (StateNode*)state_node_node;

  char* event = state_take_word(state);

  // Always transition
  if(event == NULL) {
    transition_node->type = TRANSITION_IMMEDIATE_TYPE;

    // Currently in a call, so rewind to back out.
    state_rewind(state, 2);
  } else {
    unsigned short key = keyword_get(event);

    switch(key) {
      case KW_DELAY: {
        _check(parser_consume_inline_delay(state, transition_node, state_node));
        break;
      }
      case KW_ON: {
        _check(consume_on(state, transition_node));
        link_event_transition(state_node, transition_node);
        break;
      }
      default: {
        IdentifierExpression* identifier_expression = node_create_identifierexpression();
        identifier_expression->name = event;
        transition_node->event = (Expression*)identifier_expression;
        link_event_transition(state_node, transition_node);
        break;
      }
    }
  }

  Node* transition_node_node = (Node*)transition_node;
  int rewind_to_start = event == NULL ? 2 : strlen(event);
  state_node_start_pos(state, transition_node_node, rewind_to_start);

  switch(current_node_type) {
    case NODE_STATE_TYPE: {
      node_append(current_node, transition_node_node);
      state->parent_node = current_node;
      break;
    }
    case NODE_INVOKE_TYPE: {
      node_append(current_node, transition_node_node);
      state->parent_node = current_node;
      break;
    }
    case NODE_TRANSITION_TYPE: {
      error_msg_with_code_block(state, transition_node_node, "A transition sibiling to another, hasn't happened before, this is likely a compiler bug.");
      return 2;
    }
    default: {
      error_msg_with_code_block(state, transition_node_node, "Unexpected parent node to a transition.");
      return 2;
    }
  }

  state->node = transition_node_node;

  bool block_transition = false;
  char* identifier = NULL;
  while(true) {
    int token = consume_token(state);

    if(token == TOKEN_EOL) {
      if(block_transition) {
        token = consume_token(state);

        while(token == TOKEN_EOL) {
          token = consume_token(state);
        }

        if(token != TOKEN_END_BLOCK) {
          error_msg_with_code_block(state, transition_node_node, "Block transition expects destination state.");
          err = 2;
          goto end;
        }

        break;
      }
      // End of the transition
      break;
    }

    if(token != TOKEN_CALL) {
      error_msg_with_code_block_dec(state, state->token_len, "Expected a destination state for this event.");
      err = 2;
      goto end;
    }

    token = consume_token(state);

    while(block_transition && token == TOKEN_EOL) {
      token = consume_token(state);
    }

    if(token == TOKEN_BEGIN_BLOCK && !block_transition) {
      block_transition = true;
      token = consume_token(state);

      while(token == TOKEN_EOL) {
        token = consume_token(state);
      }
    }

    if(token != TOKEN_IDENTIFIER) {
      error_unexpected_identifier(state, transition_node_node);
      err = 2;
      goto end;
    }

    identifier = state_take_word(state);
    unsigned short key = keyword_get(identifier);
    switch(key) {
      // Inline guard
      case KW_GUARD: {
        _check(parser_consume_inline_guard(state, transition_node));
        free(identifier);
        break;
      }
      case KW_ASSIGN: {
        _check(parser_consume_inline_assign(state, transition_node_node));
        free(identifier);
        break;
      }
      case KW_ACTION: {
        _check(parser_consume_inline_action(state, transition_node_node));
        free(identifier);
        break;
      }
      case KW_SEND: {
        _check(parser_consume_inline_send(state, transition_node_node));
        free(identifier);
        break;
      }
      default: {
        if(state_has_guard(state, identifier)) {
          node_transition_add_guard(transition_node, identifier);
        } else if(state_has_action(state, identifier)) {
          node_transition_add_action(transition_node, identifier);
        } else {
          transition_node->dest = identifier;
        }
        break;
      }
    }
  }

  end: {
    state_node_up(state);
    return err;
  }
}