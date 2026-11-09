#include "internal.h"


ParseStatus parser_consume_state(Parser* parser) {
  ParseStatus status = PARSE_OK;

  StateNode* state_node = node_create_state(parser->arena);
  Node* state_node_node = (Node*)state_node;

  Node* parent_node = parser->node;
  if(parent_node == NULL || parent_node->type != NODE_MACHINE) {
    parser_error(parser, "Unexpected parent node for state.");
    return PARSE_FATAL;
  }

  parser_enter_node(parser, state_node_node);

  TokenType token = parser_next_token(parser);

  switch(token) {
    case TOKEN_IDENTIFIER: {
      // Set the name of the state
      state_node->name = (SourceSpan){ .start = parser->word_start, .end = parser->word_end };

      switch(parser->modifier) {
        case MODIFIER_INITIAL: {
          parser->modifier = MODIFIER_NONE;
          MachineNode* machine_node = (MachineNode*)parent_node;
          machine_node->has_initial = true;
          machine_node->initial = state_node->name;
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
      status = PARSE_ERROR;
      parser_error_back(parser, parser->token_len, "States must be given a name.");
      break;
    }
  }

  if(token != TOKEN_BEGIN_BLOCK) {
    parser_error_unexpected_identifier(parser);
    return PARSE_FATAL;
  }

  while(true) {
    token = parser_next_token(parser);

    switch(token) {
      case TOKEN_EOL: continue;
      case TOKEN_END_BLOCK: {
        goto end;
      };
      case TOKEN_CALL: {
        parser_set_word(parser, NULL, 0, 0);
        if(parse_merge(&status, parser_consume_transition(parser)) == PARSE_FATAL) {
          return PARSE_FATAL;
        }
        break;
      }
      case TOKEN_IDENTIFIER: {
        Keyword key = keyword_lookup(parser->word);

        switch(key) {
          case KEYWORD_INVOKE: {
            if(parse_merge(&status, parser_consume_invoke(parser)) == PARSE_FATAL) {
              return PARSE_FATAL;
            }
            break;
          }
          case KEYWORD_MACHINE: {
            if(parse_merge(&status, parser_consume_machine(parser)) == PARSE_FATAL) {
              return PARSE_FATAL;
            }
            break;
          }
          default: {
            if(parse_merge(&status, parser_consume_transition(parser)) == PARSE_FATAL) {
              return PARSE_FATAL;
            }
            break;
          }
        }

        break;
      }
      case TOKEN_LOCAL: {
        const char* local = parser_take_word(parser);
        LocalKind key = local_lookup(local);
        switch(key) {
          case LOCAL_ENTRY:
          case LOCAL_EXIT: {
            if(parse_merge(&status, parser_consume_local(parser, key)) == PARSE_FATAL) {
              return PARSE_FATAL;
            }
            break;
          }
          default: {
            parser_error_backf(parser, parser->token_len, "Unknown local variable (%s)", local);
            status = PARSE_ERROR;
            break;
          }
        }
        break;
      }
      default: {
        // Known bug: this reports the error but returns the status so far,
        // which may be PARSE_OK, so the compile can still succeed.
        parser_error_unexpected_identifier(parser);
        goto end;
      }
    }
  }

  end: {
    parser_leave_node(parser);
    return status;
  }
}