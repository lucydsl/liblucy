#include "internal.h"

#include <stdio.h>
#include <string.h>

int parser_consume_state(Parser* parser) {
  int err = 0;

  StateNode* state_node = node_create_state(parser->arena);
  Node* state_node_node = (Node*)state_node;
  parser_node_start_pos(parser, state_node_node, 5);

  Node* parent_node = parser->node;
  if(parent_node == NULL || parent_node->type != NODE_MACHINE) {
    parser_error(parser, state_node_node, "Unexpected parent node for state.");
    return 2;
  }

  parser_enter_node(parser, state_node_node);

  TokenType token = parser_next_token(parser);

  switch(token) {
    case TOKEN_IDENTIFIER: {
      // Set the name of the state
      state_node->name_start = parser->word_start;
      state_node->name_end = parser->word_end;

      switch(parser->modifier) {
        case MODIFIER_INITIAL: {
          parser->modifier = MODIFIER_NONE;
          MachineNode* machine_node = (MachineNode*)parent_node;
          machine_node->initial_start = state_node->name_start;
          machine_node->initial_end = state_node->name_end;
          break;
        }
        case MODIFIER_FINAL: {
          parser->modifier = MODIFIER_NONE;
          state_node->final = true;
          break;
        }
        default: {
          break;
        }
      }

      token = parser_next_token(parser);
      break;
    }
    default: {
      err = 1;
      parser_error_back(parser, parser->token_len, "States must be given a name.");
      break;
    }
  }

  if(token != TOKEN_BEGIN_BLOCK) {
    parser_error_unexpected_identifier(parser, state_node_node);
    return 2;
  }

  while(true) {
    token = parser_next_token(parser);

    switch(token) {
      case TOKEN_EOL: continue;
      case TOKEN_END_BLOCK: {
        state_node_node->end = parser->index;
        goto end;
      };
      case TOKEN_CALL: {
        parser_set_word(parser, NULL, 0, 0);
        _check(parser_consume_transition(parser));
        break;
      }
      case TOKEN_IDENTIFIER: {
        Keyword key = keyword_lookup(parser->word);

        switch(key) {
          case KEYWORD_INVOKE: {
            _check(parser_consume_invoke(parser));
            break;
          }
          case KEYWORD_MACHINE: {
            _check(parser_consume_machine(parser));
            break;
          }
          default: {
            _check(parser_consume_transition(parser));
            break;
          }
        }

        break;
      }
      case TOKEN_LOCAL: {
        char* local = parser_take_word(parser);
        LocalKind key = local_lookup(local);
        switch(key) {
          case LOCAL_ENTRY:
          case LOCAL_EXIT: {
            _check(parser_consume_local(parser, key));
            break;
          }
          default: {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "Unknown local variable (%s)", local);
            parser_error_back(parser, parser->token_len, buffer);
            err = 1;
            break;
          }
        }
        break;
      }
      default: {
        parser_error_unexpected_identifier(parser, state_node_node);
        goto end;
      }
    }
  }

  end: {
    parser_leave_node(parser);
    return err;
  }
}