#include "internal.h"

static int consume_machine_inner(Parser* parser, bool is_implicit, TokenType initial_token) {
  int err = 0;
  TokenType token = is_implicit ? initial_token : parser_next_token(parser);

  while(true) {
    switch(token) {
      case TOKEN_EOL: goto next;
      case TOKEN_EOF: goto end;
      case TOKEN_END_BLOCK: {
        if(!is_implicit) {
          goto end;
        }
        parser_error_unexpected_identifier(parser, parser->node);
        err = 1;
        break;
      }
      case TOKEN_IDENTIFIER: {
        Keyword key = keyword_lookup(parser->word);
        if(key == KEYWORD_NONE) {
          parser_error(parser, parser->node, "Unknown top-level identifier.");
          err = 2;
          goto end;
        }

        switch(key) {
          case KEYWORD_INITIAL: {
            parser->modifier = MODIFIER_INITIAL;
            break;
          }
          case KEYWORD_FINAL: {
            parser->modifier = MODIFIER_FINAL;
            break;
          }
          case KEYWORD_STATE: {
            _check(parser_consume_state(parser));
            break;
          }
          case KEYWORD_USE: {
            _check(parser_consume_use(parser));
            break;
          }
          case KEYWORD_ACTION: {
            _check(parser_consume_action(parser));
            break;
          }
          case KEYWORD_GUARD: {
            _check(parser_consume_guard(parser));
            break;
          }
          default: {
            break;
          }
        }

        break;
      }
      default: {
        parser_error_unexpected_identifier(parser, parser->node);
        err = 2;
        goto end;
      }
    }

    next: {
      token = parser_next_token(parser);
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

static int consume_named_machine(Parser* parser) {
  int err = 0;

  MachineNode* machine_node = node_create_machine(parser->arena);
  Node* node = (Node*)machine_node;
  parser_enter_node(parser, node);

  TokenType token = parser_next_token(parser);
  if(token != TOKEN_IDENTIFIER) {
    parser_error(parser, node, "Machine must have a name.");
    err = 1;
    goto end;
  }
  machine_node->name_start = parser->word_start;
  machine_node->name_end = parser->word_end;

  token = parser_next_token(parser);

  if(token != TOKEN_BEGIN_BLOCK) {
    parser_error_unexpected_identifier(parser, node);
    err = 1;
    goto end;
  }

  _check(consume_machine_inner(parser, false, 0));

  end: {
    parser_leave_node(parser);
    return err;
  }
}

/* Public API */
int parser_consume_machine(Parser* parser) {
  if(parser->machine_depth >= MAX_MACHINE_DEPTH) {
    parser_error_back(parser, parser->token_len, "Machines are nested too deeply.");
    return 2;
  }

  parser->machine_depth++;
  int err = consume_named_machine(parser);
  parser->machine_depth--;
  return err;
}

// Only at the top level, so it can't be too deep.
int parser_consume_implicit_machine(Parser* parser, TokenType current_token) {
  int err = 0;

  MachineNode* machine_node = node_create_machine(parser->arena);
  Node* node = (Node*)machine_node;

  parser_enter_node(parser, node);

  parser->machine_depth++;
  int inner_err = consume_machine_inner(parser, true, current_token);
  parser->machine_depth--;
  _check(inner_err);

  parser_leave_node(parser);
  return err;
}