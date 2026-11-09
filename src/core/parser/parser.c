#include "parser.h"

#include "internal.h"

static int consume_program(State* state) {
  int err = 0;
  Program* program = state->program;

  TokenType token;

  while(true) {
    token = consume_token(state);

    switch(token) {
      case TOKEN_EOL: continue;
      case TOKEN_EOF: goto end;
      case TOKEN_IDENTIFIER: {
        char* identifier = state->word;

        if(!is_keyword(identifier)) {
          error_msg_with_code_block(state, state->node, "Unknown top-level identifier.");
          return 2;
        }

        Keyword key = keyword_get(identifier);
        switch(key) {
          case KEYWORD_USE: {
            _check(parser_consume_use(state));
            break;
          }
          case KEYWORD_MACHINE: {
            _check(parser_consume_machine(state));
            break;
          }
          default: {
            // Top-level machine
            _check(parser_consume_implicit_machine(state, token));
            break;
          }
        }

        break;
      }
      default: {
        break;
      }
    }
  }

  end: {
    return err;
  }
}

ParseResult parse(Arena* arena, char* source, char* filename) {
  Program* program = new_program(arena);
  State state;
  state_init(&state, arena, program, source, filename);

  int err = consume_program(&state);

  ParseResult result = {
    .success = err == 0,
    .program = program
  };
  return result;
}
