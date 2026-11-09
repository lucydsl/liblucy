#include "internal.h"

static int consume_machine_inner(State* state, bool is_implicit, int initial_token) {
  int err = 0;
  int token = is_implicit ? initial_token : consume_token(state);

  while(true) {
    switch(token) {
      case TOKEN_EOL: goto next;
      case TOKEN_EOF: goto end;
      case TOKEN_END_BLOCK: {
        if(!is_implicit) {
          goto end;
        }
        error_unexpected_identifier(state, state->node);
        err = 1;
        break;
      }
      case TOKEN_IDENTIFIER: {
        char* identifier = state->word;

        if(!is_keyword(identifier)) {
          error_msg_with_code_block(state, state->node, "Unknown top-level identifier.");
          err = 2;
          goto end;
        }

        unsigned short key = keyword_get(identifier);
        switch(key) {
          case KW_INITIAL: {
            state->modifier = MODIFIER_TYPE_INITIAL;
            break;
          }
          case KW_FINAL: {
            state->modifier = MODIFIER_TYPE_FINAL;
            break;
          }
          case KW_STATE: {
            _check(parser_consume_state(state));
            break;
          }
          case KW_USE: {
            _check(parser_consume_use(state));
            break;
          }
          case KW_ACTION: {
            _check(parser_consume_action(state));
            break;
          }
          case KW_GUARD: {
            _check(parser_consume_guard(state));
            break;
          }
        }

        break;
      }
      default: {
        error_unexpected_identifier(state, state->node);
        err = 2;
        goto end;
      }
    }

    next: {
      token = consume_token(state);
    }
  }

  end: {
    return err;
  }
}

// Machines nest inside states, and the parser recurses through
// consume_machine_inner and parser_consume_state for each level, so deep
// enough nesting ran out of stack. In wasm, where the stack is 64 KB, about
// 150 levels did, and the trap left the module unusable. Nobody nests
// machines anywhere near this deep.
#define MAX_MACHINE_DEPTH 64

static int consume_named_machine(State* state) {
  int err = 0;

  MachineNode* machine_node = node_create_machine(state->arena);
  Node* node = (Node*)machine_node;
  state_node_set(state, node);

  int token = consume_token(state);
  if(token != TOKEN_IDENTIFIER) {
    error_msg_with_code_block(state, node, "Machine must have a name.");
    err = 1;
    goto end;
  }
  machine_node->name_start = state->word_start;
  machine_node->name_end = state->word_end;

  token = consume_token(state);

  if(token != TOKEN_BEGIN_BLOCK) {
    error_unexpected_identifier(state, node);
    err = 1;
    goto end;
  }

  _check(consume_machine_inner(state, false, 0));

  end: {
    state_node_up(state);
    return err;
  }
}

/* Public API */
int parser_consume_machine(State* state) {
  if(state->machine_depth >= MAX_MACHINE_DEPTH) {
    error_msg_with_code_block_dec(state, state->token_len, "Machines are nested too deeply.");
    return 2;
  }

  state->machine_depth++;
  int err = consume_named_machine(state);
  state->machine_depth--;
  return err;
}

// Only at the top level, so it can't be too deep.
int parser_consume_implicit_machine(State* state, int current_token) {
  int err = 0;

  MachineNode* machine_node = node_create_machine(state->arena);
  Node* node = (Node*)machine_node;

  state_node_set(state, node);

  state->machine_depth++;
  int inner_err = consume_machine_inner(state, true, current_token);
  state->machine_depth--;
  _check(inner_err);

  state_node_up(state);
  return err;
}