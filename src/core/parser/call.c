#include "internal.h"

#include <stdbool.h>
#include <stdio.h>

int consume_call_expression(State* state, const char* fn_name, void* expr, consume_call_expr_args on_args) {
  int err = 0;
  int token = consume_token(state);

  bool in_call = false;
  if(token != TOKEN_BEGIN_CALL) {
    if(token == TOKEN_IDENTIFIER) {
      char buffer[256];
      snprintf(buffer, sizeof(buffer), "%s must be called like a function. Expected %s(%s)", fn_name, fn_name, state->word);
      error_msg_with_code_block_dec(state, state->word_len, buffer);
    } else if(token == TOKEN_CALL) {
      char buffer[256];
      snprintf(buffer, sizeof(buffer), "%s must be called like a function.", fn_name);
      error_msg_with_code_block_dec(state, state->token_len, buffer);
    } else {
      // What should this be?
      error_msg_with_code_block(state, NULL, "Expected a (");
    }
    err = 1;
  } else {
    in_call = true;
    token = consume_token(state);
  }

  char* identifier = NULL;
  int argi = 0;
  while(token != TOKEN_END_CALL) {
    switch(token) {
      case TOKEN_COMMA: break;
      case TOKEN_IDENTIFIER:
      case TOKEN_INTEGER:
      case TOKEN_SYMBOL:
      case TOKEN_TIMEFRAME: {
        identifier = state_take_word(state);
        _check(on_args(state, expr, token, identifier, argi));
        break;
      }
      default: {
        if(in_call) {
          error_msg_with_code_block_dec(state, state->token_len, "Unknown function argument.");
          err = 2;
          goto end;
        }
        goto end_args;
      }
    }
    token = consume_token(state);
    argi++;
  }

  end_args: {
    if(token != TOKEN_END_CALL) {
      // Only show this error message if we are currently within a call.
      if(in_call) {
        char buffer[256];
        char* key = identifier == NULL ? "" : identifier;
        snprintf(buffer, sizeof(buffer), "Inline assigns must be called like a function. Expected assign(%s)", key);
        error_msg_with_code_block_dec(state, state->token_len, buffer);
      }

      // Rewind back out, so the next thing consumes this token.
      state_rewind(state, state->token_len);

      err = 1;
    }
  }

  end: {
    return err;
  }
}

int expect_call_argument(State* state, const char* fn_name, const void* arg, int err) {
  if(arg != NULL) {
    return err;
  }
  if(err == 0) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s() needs an argument.", fn_name);
    error_msg_with_code_block_dec(state, state->token_len, buffer);
    return 1;
  }
  return err;
}
