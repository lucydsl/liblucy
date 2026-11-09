#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "keyword.h"
#include "local_kind.h"
#include "../ast/node.h"
#include "lexer.h"
#include "action.h"
#include "assign.h"
#include "core.h"
#include "send.h"
#include "token.h"

int parser_consume_local(State* state, unsigned short key) {
  int err = 0;

  LocalNode* local_node = node_create_local(state->arena);
  Node* node = (Node*)local_node;
  local_node->key = key;
  state_node_start_pos(state, node, state->token_len);

  Node* parent_node = state->node;
  if(parent_node == NULL || parent_node->type != NODE_STATE_TYPE) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "Unexpected parent of %s", local_get_name(key));
    error_msg_with_code_block_dec(state, state->token_len, buffer);
    err = 1;
  }

  state_node_set(state, node);

  // Only a state has entry and exit slots; writing them into another
  // kind of node would write past its end.
  if(parent_node != NULL && parent_node->type == NODE_STATE_TYPE) {
    StateNode* parent_state_node = (StateNode*)parent_node;
    if(key == LOCAL_ENTRY) {
      parent_state_node->entry = local_node;
    } else {
      parent_state_node->exit = local_node;
    }
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
    // Not "key": that would hide the @entry/@exit key, which the guard
    // error below needs (it printed local_get_name(KW_GUARD), a NULL).
    unsigned short keyword = keyword_get(identifier);
    switch(keyword) {
      case KW_ACTION: {
        _check(parser_consume_inline_action(state, node));
        break;
      }
      case KW_ASSIGN: {
        _check(parser_consume_inline_assign(state, node));
        break;
      }
      case KW_SEND: {
        _check(parser_consume_inline_send(state, node));
        break;
      }
      case KW_GUARD: {
        err = 1;
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Guards are not allowed in %s", local_get_name(key));
        error_msg_with_code_block_dec(state, state->token_len, buffer);
        break;
      }
      // Not a keyword
      case KW_NOT_A_KEYWORD: {
        if(state_has_action(state, identifier)) {
          TransitionAction* action = create_transition_action(state->arena);
          action->name = identifier;
          node_local_add_action(local_node, action);
        } else {
          err = 1;
          char buffer[256];
          snprintf(buffer, sizeof(buffer), "Unknown action [%s]. Did you mean to declare this action at the top of the machine?", identifier);
          error_msg_with_code_block_dec(state, state->token_len, buffer);
        }
        break;
      }
      default: {
        error_msg_with_code_block_dec(state, state->token_len, "Unexpected keyword in action assignment.");
        err = 1;
        break;
      }
    }
  }

  // `@entry` on its own has no actions, and the printer crashed on the
  // empty list.
  if(err == 0 && local_node->action == NULL) {
    error_msg_with_code_block_dec(state, state->token_len, "Expected a => here.");
    err = 1;
  }

  end: {
    state_node_up(state);
    return err;
  }
}