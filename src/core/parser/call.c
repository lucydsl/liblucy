#include "internal.h"

#include <stdbool.h>

ParseStatus parser_consume_call(Parser* parser, const char* fn_name, void* expr, CallArgHandler on_args) {
  ParseStatus status = PARSE_OK;
  TokenType token = parser_next_token(parser);

  bool in_call = false;
  if(token != TOKEN_BEGIN_CALL) {
    if(token == TOKEN_IDENTIFIER) {
      parser_error_backf(parser, parser->word_len, "%s must be called like a function. Expected %s(%s)", fn_name, fn_name, parser->word);
    } else if(token == TOKEN_CALL) {
      parser_error_backf(parser, parser->token_len, "%s must be called like a function.", fn_name);
    } else {
      // What should this be?
      parser_error(parser, "Expected a (");
    }
    status = PARSE_ERROR;
  } else {
    in_call = true;
    token = parser_next_token(parser);
  }

  const char* identifier = NULL;
  size_t index = 0;
  while(token != TOKEN_END_CALL) {
    switch(token) {
      case TOKEN_COMMA: break;
      case TOKEN_IDENTIFIER:
      case TOKEN_INTEGER:
      case TOKEN_SYMBOL:
      case TOKEN_TIMEFRAME: {
        identifier = parser_take_word(parser);
        if(parse_merge(&status, on_args(parser, expr, token, identifier, index)) == PARSE_FATAL) {
          return PARSE_FATAL;
        }
        break;
      }
      default: {
        if(in_call) {
          parser_error_back(parser, parser->token_len, "Unknown function argument.");
          status = PARSE_FATAL;
          goto end;
        }
        goto end_args;
      }
    }
    token = parser_next_token(parser);
    index++;
  }

  end_args: {
    if(token != TOKEN_END_CALL) {
      // Only show this error message if we are currently within a call.
      if(in_call) {
        const char* key = identifier == NULL ? "" : identifier;
        parser_error_backf(parser, parser->token_len, "Inline assigns must be called like a function. Expected assign(%s)", key);
      }

      // Rewind back out, so the next thing consumes this token.
      parser_rewind(parser, parser->token_len);

      status = PARSE_ERROR;
    }
  }

  end: {
    return status;
  }
}

ParseStatus parser_expect_call_argument(Parser* parser, const char* fn_name, const void* arg, ParseStatus status) {
  if(arg != NULL) {
    return status;
  }
  if(status == PARSE_OK) {
    parser_error_backf(parser, parser->token_len, "%s() needs an argument.", fn_name);
    return PARSE_ERROR;
  }
  return status;
}
