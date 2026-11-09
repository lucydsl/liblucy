#include "internal.h"

#include <string.h>

static int consume_use_specifiers(ImportNode* import_node, State* state) {
  int err = 0;
  int token;

  // The specifier read but not yet added. Only add one once: `{ a,, b }`
  // used to append a twice, which made the list loop, and `{ }` appended
  // an uninitialised pointer.
  ImportSpecifier *specifier = NULL;
  while(true) {
    token = consume_token(state);
    
    switch(token) {
      case TOKEN_IDENTIFIER: {
        char* identifier = state_take_word(state);
        if(strcmp(identifier, "as") == 0) {

          error_msg_with_code_block(state, (Node*)import_node, "Import aliases are not currently supported.");
          return 2;
        }

        specifier = node_create_import_specifier(state->arena, identifier);
        break;
      }
      case TOKEN_END_BLOCK: {
        if(specifier != NULL) {
          node_append((Node*)import_node, (Node*)specifier);
        }
        goto end;
      }
      case TOKEN_EOL: {
        break;
      }
      case TOKEN_COMMA: {
        if(specifier != NULL) {
          node_append((Node*)import_node, (Node*)specifier);
          specifier = NULL;
        }
        break;
      }
      default: {
        error_unexpected_identifier(state, (Node*)import_node);
        goto end;
      }
    }
  }

  end: {
    return err;
  }
}

int parser_consume_use(State* state) {
  int err = 0;
  Node *current_node = state->node;

  if(current_node != NULL) {
    error_msg_with_code_block(state, current_node, "Import statement must be at the top of the file.");
    // Return rather than go to end: no node has been pushed yet, so
    // state_node_up would pop the enclosing machine, and the next state
    // would find no parent.
    return 1;
  }

  ImportNode *import_node = node_create_import_statement(state->arena);
  Node *node = (Node*)import_node;
  state_node_start_pos(state, node, 4);
  state_node_set(state, node);

  int token;

  token = consume_token(state);

  // use './util'
  if(token != TOKEN_STRING) {
    error_msg_with_code_block(state, current_node, "Expected a string.");
    err = 1;
    goto end;
  }
  import_node->from = state_take_word(state);

  // Start grabbing specififers
  bool consumed_loc = false;
  bool consumed_specifiers = false;
  bool consumed_from = false;
  loop: while(true) {
    token = consume_token(state);

    switch(token) {
      case TOKEN_EOL: {
        if(consumed_specifiers) {
          goto end;
        }
      };
      case TOKEN_BEGIN_BLOCK: {
        _check(consume_use_specifiers(import_node, state));
        consumed_specifiers = true;

        break;
      }
      case TOKEN_IDENTIFIER: {
        error_unexpected_identifier(state, node);
        err = 1;
        goto end;
      }
      case TOKEN_UNKNOWN: {
        error_unexpected_identifier(state, node);
        err = 2;
        goto end;
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