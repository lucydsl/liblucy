#include <stdlib.h>
#include "../error.h"
#include "../keyword.h"
#include "../local.h"
#include "../program.h"
#include "../state.h"
#include "../timeframe.h"
#include "core.h"
#include "machine.h"
#include "parser.h"
#include "token.h"
#include "use.h"

static int consume_program(State* state) {
  int err = 0;
  Program* program = state->program;

  int token;

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

        unsigned short key = keyword_get(identifier);
        switch(key) {
          case KW_USE: {
            _check(parser_consume_use(state));
            break;
          }
          case KW_MACHINE: {
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
    }
  }

  end: {
    return err;
  }
}

ParseResult* parse(char* source, char* filename) {
  int err = 0;
  Program* program = new_program();
  State* state = state_new_state(source, filename);
  state->program = program;

  err = consume_program(state);

  ParseResult *result = malloc(sizeof(*result));
  result->success = err == 0;
  result->program = program;

  return result;
}

void parser_init() {
  keyword_init();
  local_init();
  timeframe_init();
}