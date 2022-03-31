#include <stdio.h>
#include <string.h>
#include "../error.h"
#include "../keyword.h"
#include "../local.h"
#include "../node.h"
#include "../state.h"
#include "core.h"
#include "invoke.h"
#include "local.h"
#include "machine.h"
#include "token.h"
#include "transition.h"

int parser_consume_state(State* state) {
  int err = 0;

  StateNode* state_node = node_create_state();
  Node* state_node_node = (Node*)state_node;
  state_node_start_pos(state, state_node_node, 5);

  Node* parent_node = state->node;
  if(parent_node->type != NODE_MACHINE_TYPE) {
    error_msg_with_code_block(state, state_node_node, "Unexpected parent node for state.");
    return 2;
  }

  state_node_set(state, state_node_node);

  int token = consume_token(state);

  switch(token) {
    case TOKEN_IDENTIFIER: {
      // Set the name of the state
      char* state_name = state_take_word(state);
      state_node->name_start = state->word_start;
      state_node->name_end = state->word_end;

      switch(state->modifier) {
        case MODIFIER_TYPE_INITIAL: {
          state->modifier = MODIFIER_NONE;
          MachineNode* machine_node = (MachineNode*)parent_node;
          machine_node->initial = strdup(state_name);
          break;
        }
        case MODIFIER_TYPE_FINAL: {
          state->modifier = MODIFIER_NONE;
          state_node->final = true;
          break;
        }
      }

      token = consume_token(state);
      break;
    }
    default: {
      err = 1;
      error_msg_with_code_block_dec(state, state->token_len, "States must be given a name.");
      break;
    }
  }

  if(token != TOKEN_BEGIN_BLOCK) {
    error_unexpected_identifier(state, state_node_node);
    return 2;
  }

  while(true) {
    token = consume_token(state);

    switch(token) {
      case TOKEN_EOL: continue;
      case TOKEN_END_BLOCK: {
        state_node_node->end = state->index;
        goto end;
      };
      case TOKEN_CALL: {
        state_set_word(state, NULL, 0, 0);
        _check(parser_consume_transition(state));
        break;
      }
      case TOKEN_IDENTIFIER: {
        unsigned short key = keyword_get(state->word);

        switch(key) {
          case KW_INVOKE: {
            _check(parser_consume_invoke(state));
            break;
          }
          case KW_MACHINE: {
            _check(parser_consume_machine(state));
            break;
          }
          default: {
            _check(parser_consume_transition(state));
            break;
          }
        }

        break;
      }
      case TOKEN_LOCAL: {
        char* local = state_take_word(state);
        unsigned short key = local_get(local);
        switch(key) {
          case LOCAL_ENTRY:
          case LOCAL_EXIT: {
            _check(parser_consume_local(state, key));
            break;
          }
          default: {
            char buffer[100];
            sprintf(buffer, "Unknown local variable (%s)", state->word);
            error_msg_with_code_block_dec(state, state->token_len, buffer);
            err = 1;
            break;
          }
        }
        break;
      }
      default: {
        error_unexpected_identifier(state, state_node_node);
        goto end;
      }
    }
  }

  end: {
    state_node_up(state);
    return err;
  }
}