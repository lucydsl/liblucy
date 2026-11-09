#include "internal.h"

#include <stdbool.h>
#include <stdio.h>

int parser_consume_call(Parser* parser, const char* fn_name, void* expr, CallArgHandler on_args) {
  int err = 0;
  TokenType token = parser_next_token(parser);

  bool in_call = false;
  if(token != TOKEN_BEGIN_CALL) {
    if(token == TOKEN_IDENTIFIER) {
      char buffer[256];
      snprintf(buffer, sizeof(buffer), "%s must be called like a function. Expected %s(%s)", fn_name, fn_name, parser->word);
      parser_error_back(parser, parser->word_len, buffer);
    } else if(token == TOKEN_CALL) {
      char buffer[256];
      snprintf(buffer, sizeof(buffer), "%s must be called like a function.", fn_name);
      parser_error_back(parser, parser->token_len, buffer);
    } else {
      // What should this be?
      parser_error(parser, NULL, "Expected a (");
    }
    err = 1;
  } else {
    in_call = true;
    token = parser_next_token(parser);
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
        identifier = parser_take_word(parser);
        _check(on_args(parser, expr, token, identifier, argi));
        break;
      }
      default: {
        if(in_call) {
          parser_error_back(parser, parser->token_len, "Unknown function argument.");
          err = 2;
          goto end;
        }
        goto end_args;
      }
    }
    token = parser_next_token(parser);
    argi++;
  }

  end_args: {
    if(token != TOKEN_END_CALL) {
      // Only show this error message if we are currently within a call.
      if(in_call) {
        char buffer[256];
        char* key = identifier == NULL ? "" : identifier;
        snprintf(buffer, sizeof(buffer), "Inline assigns must be called like a function. Expected assign(%s)", key);
        parser_error_back(parser, parser->token_len, buffer);
      }

      // Rewind back out, so the next thing consumes this token.
      parser_rewind(parser, parser->token_len);

      err = 1;
    }
  }

  end: {
    return err;
  }
}

int parser_expect_call_argument(Parser* parser, const char* fn_name, const void* arg, int err) {
  if(arg != NULL) {
    return err;
  }
  if(err == 0) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s() needs an argument.", fn_name);
    parser_error_back(parser, parser->token_len, buffer);
    return 1;
  }
  return err;
}
