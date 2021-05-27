#include <stdio.h>
#include <stdlib.h>
#include "../error.h"
#include "../keyword.h"
#include "../local.h"
#include "../node.h"
#include "../state.h"
#include "action.h"
#include "assign.h"
#include "core.h"
#include "send.h"
#include "token.h"

int parser_consume_local(State* state, unsigned short key) {
  int err = 0;

  LocalNode* local_node = node_create_local();
  Node* node = (Node*)local_node;
  local_node->key = key;
  state_node_start_pos(state, node, state->token_len);

  Node* parent_node = state->node;
  if(parent_node->type != NODE_STATE_TYPE) {
    char buffer[100];
    sprintf(buffer, "Unexpected parent of %s", local_get_name(key));
    error_msg_with_code_block_dec(state, state->token_len, buffer);
    err = 1;
  }

  state_node_set(state, node);

  StateNode* parent_state_node = (StateNode*)parent_node;
  if(key == LOCAL_ENTRY) {
    parent_state_node->entry = local_node;
  } else {
    parent_state_node->exit = local_node;
  }

  int token;
  while(true) {
    token = consume_token(state);

    if(token == TOKEN_EOL) {
      break;
    }

    if(token != TOKEN_CALL) {
      error_msg_with_code_block_dec(state, state->token_len, "Expected a => here.");
      err = 1;
    }

    token = consume_token(state);

    if(token != TOKEN_IDENTIFIER) {
      error_msg_with_code_block_dec(state, state->token_len, "Expected an action.");
      err = 2;
      goto end;
    }

    char* identifier = state_take_word(state);
    unsigned short key = keyword_get(identifier);
    switch(key) {
      case KW_ACTION: {
        _check(parser_consume_inline_action(state, node));
        free(identifier);
        break;
      }
      case KW_ASSIGN: {
        _check(parser_consume_inline_assign(state, node));
        free(identifier);
        break;
      }
      case KW_SEND: {
        _check(parser_consume_inline_send(state, node));
        free(identifier);
        break;
      }
      case KW_GUARD: {
        err = 1;
        char buffer[100];
        sprintf(buffer, "Guards are not allowed in %s", local_get_name(key));
        error_msg_with_code_block_dec(state, state->token_len, buffer);
        free(identifier);
        break;
      }
      // Not a keyword
      case KW_NOT_A_KEYWORD: {
        if(state_has_action(state, identifier)) {
          TransitionAction* action = create_transition_action();
          action->name = identifier;
          node_local_add_action(local_node, action);
        } else {
          err = 1;
          char buffer[100];
          sprintf(buffer, "Unknown action [%s]. Did you mean to declare this action at the top of the machine?", identifier);
          error_msg_with_code_block_dec(state, state->token_len, buffer);
          free(identifier);
        }
        break;
      }
      default: {
        error_msg_with_code_block_dec(state, state->token_len, "Unexpected keyword in action assignment.");
        err = 1;
        free(identifier);
        break;
      }
    }
  }

  end: {
    state_node_up(state);
    return err;
  }
}