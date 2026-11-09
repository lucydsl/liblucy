#include "internal.h"

#include <stdlib.h>
#include <string.h>

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

static void link_event_transition(Node* parent_node, TransitionNode* transition_node) {
  char* event_name = get_event_name(transition_node);

  TransitionNode* child_transition_node = NULL;
  switch(parent_node->type) {
    case NODE_STATE_TYPE: {
      StateNode* state_node = (StateNode*)parent_node;
      child_transition_node = state_node->event_transition;
      if(child_transition_node == NULL) {
        state_node->event_transition = transition_node;
        return;
      }
      break;
    }
    case NODE_INVOKE_TYPE: {
      InvokeNode* invoke_node = (InvokeNode*)parent_node;
      child_transition_node = invoke_node->event_transition;
      if(child_transition_node == NULL) {
        invoke_node->event_transition = transition_node;
        return;
      }
      break;
    }
  }

  if(child_transition_node != NULL) {
    // Transitions for the same event are chained through link, starting
    // from the first one. Only link into that first chain: the later
    // transitions are in it already, and linking from them too would make
    // the chain loop back on itself (with three or more, it did, and the
    // printer never finished).
    bool linked_already = false;
    do {
      char* name = get_event_name(child_transition_node);
      if(!linked_already && name != NULL && event_name != NULL &&
        strcmp(name, event_name) == 0) {
        TransitionNode* linked = child_transition_node;
        while(linked->link != NULL) {
          linked = linked->link;
        }
        linked->link = transition_node;
        linked_already = true;
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

static void link_immediate_transition(StateNode* state_node, TransitionNode* transition_node) {
  if(state_node->immediate_transition == NULL) {
    state_node->immediate_transition = transition_node;
  } else {
    TransitionNode* child = state_node->immediate_transition;
    while(child->next != NULL) {
      child = child->next;
    }
    child->next = transition_node;
  }
}

static void link_delay_transition(StateNode* state_node, TransitionNode* transition_node) {
  if(state_node->delay_transition == NULL) {
    state_node->delay_transition = transition_node;
  } else {
    TransitionNode* child = state_node->delay_transition;
    while(child->next != NULL) {
      child = child->next;
    }
    child->next = transition_node;
  }
}

static int consume_on_args(State* state, void* expr, int _token, char* arg, int _argi) {
  OnExpression* on_expression = (OnExpression*)expr;
  on_expression->name = arg;
  return 0;
}

static int consume_on(State* state, TransitionNode* transition_node) {
  int err = 0;

  OnExpression* expression = node_create_onexpression(state->arena);
  _check(consume_call_expression(state, "on", expression, &consume_on_args));
  transition_node->event = (Expression*)expression;
  _check(expect_call_argument(state, "on", expression->name, err));

  return err;
}

static int consume_member_expression(State* state, TransitionNode* transition_node) {
  int err = 0;

  MemberExpression* top_member_expression = node_create_memberexpression(state->arena);
  top_member_expression->owner = state_take_word(state);
  state_next(state);
  int token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_unexpected_identifier(state, (Node*)transition_node);
    err = 1;
    goto exit;
  } else {
    IdentifierExpression* prop = node_create_identifierexpression(state->arena);
    prop->name = state_take_word(state);
    top_member_expression->property = (Expression*)prop;
    transition_node->dest = (Expression*)top_member_expression;
  }

  exit: {
    return err;
  }
}

/* Public API */
int parser_consume_transition(State* state) {
  int err = 0;
  TransitionNode* transition_node = node_create_transition(state->arena);

  Node* current_node = state->node;
  if(current_node == NULL) {
    error_msg_with_code_block(state, (Node*)transition_node, "Unexpected parent node to a transition.");
    return 2;
  }
  size_t current_node_type = current_node->type;

  char* event = state_take_word(state);

  // Always transition
  if(event == NULL) {
    if(current_node->type != NODE_STATE_TYPE) {
      error_msg_with_code_block_dec(state, state->token_len, "An immediate transitions must be a child of state");
      err = 1;
    } else {
      transition_node->type = TRANSITION_IMMEDIATE_TYPE;
      link_immediate_transition((StateNode*)current_node, transition_node);
    }

    // Currently in a call, so rewind to back out.
    state_rewind(state, 2);
  } else {
    unsigned short key = keyword_get(event);

    switch(key) {
      case KW_DELAY: {
        if(current_node->type != NODE_STATE_TYPE) {
          error_msg_with_code_block_dec(state, state->token_len, "delay must be a child of a state");
          err = 1;
        } else {
          _check(parser_consume_inline_delay(state, transition_node, (StateNode*)current_node));
          link_delay_transition((StateNode*)current_node, transition_node);
        }

        break;
      }
      case KW_ON: {
        _check(consume_on(state, transition_node));
        link_event_transition(current_node, transition_node);
        break;
      }
      default: {
        IdentifierExpression* identifier_expression = node_create_identifierexpression(state->arena);
        identifier_expression->name = event;
        transition_node->event = (Expression*)identifier_expression;
        link_event_transition(current_node, transition_node);
        break;
      }
    }
  }

  Node* transition_node_node = (Node*)transition_node;
  size_t rewind_to_start = event == NULL ? 2 : strlen(event);
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

    switch(token) {
      case TOKEN_IDENTIFIER: {
        identifier = state_take_word(state);
        unsigned short key = keyword_get(identifier);
        switch(key) {
          // Inline guard
          case KW_GUARD: {
            _check(parser_consume_inline_guard(state, transition_node));
            break;
          }
          case KW_ASSIGN: {
            _check(parser_consume_inline_assign(state, transition_node_node));
            break;
          }
          case KW_ACTION: {
            _check(parser_consume_inline_action(state, transition_node_node));
            break;
          }
          case KW_SEND: {
            _check(parser_consume_inline_send(state, transition_node_node));
            break;
          }
          default: {
            if(state_has_guard(state, identifier)) {
              node_transition_add_guard(state->arena, transition_node, identifier);
            } else if(state_has_action(state, identifier)) {
              node_transition_add_action(state->arena, transition_node, identifier);
            } else {
              IdentifierExpression* identifier_expression = node_create_identifierexpression(state->arena);
              identifier_expression->name = identifier;
              transition_node->dest = (Expression*)identifier_expression;
            }
            break;
          }
        }
        break;
      }
      case TOKEN_MEMBER_EXPRESSION: {
        _check(consume_member_expression(state, transition_node));
        break;
      }
      default: {
        error_unexpected_identifier(state, transition_node_node);
        err = 2;
        goto end;
      }
    }
  }

  // A transition needs at least one of a destination, a guard or an
  // action. Without any (an event name alone on a line, or `delay(1s)`)
  // the printer had no destination to print and crashed.
  if(err == 0 && transition_node->dest == NULL &&
    transition_node->guard == NULL && transition_node->action == NULL) {
    error_msg_with_code_block_dec(state, state->token_len, "Expected a destination state for this event.");
    err = 1;
  }

  end: {
    state_node_up(state);
    return err;
  }
}