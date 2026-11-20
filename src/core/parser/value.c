#include "internal.h"

#include <stdbool.h>
#include <string.h>

/*
 * A value: what a spawn's block gives an input, or the data of a send. It
 * is a reference, never an expression: event.data, context.key, self,
 * parent, or a function (one from `use`, or a :symbol) that is called with
 * the context and the event.
 */

static const char* const value_help =
  "A value is event.data, context.name, self, parent, or a function: one from use, or a :symbol.";

Expression* parser_consume_value(Parser* parser, TokenType token, const char* word) {
  switch(token) {
    case TOKEN_MEMBER_EXPRESSION: {
      MemberExpression* member = expression_create_member(parser->arena);
      member->owner = word;
      // Over the dot.
      parser_next(parser);
      token = parser_next_token(parser);
      if(token != TOKEN_IDENTIFIER) {
        parser_error_back(parser, parser->token_len, value_help);
        return NULL;
      }
      IdentifierExpression* property = expression_create_identifier(parser->arena);
      property->name = parser_take_word(parser);
      member->property = (Expression*)property;
      bool event_data = strcmp(member->owner, "event") == 0 && strcmp(property->name, "data") == 0;
      if(!event_data && strcmp(member->owner, "context") != 0) {
        parser_error_back(parser, parser->token_len, value_help);
        return NULL;
      }
      return (Expression*)member;
    }
    case TOKEN_IDENTIFIER: {
      IdentifierExpression* identifier = expression_create_identifier(parser->arena);
      identifier->name = word;
      return (Expression*)identifier;
    }
    case TOKEN_SYMBOL: {
      SymbolExpression* symbol = expression_create_symbol(parser->arena);
      symbol->name = word;
      return (Expression*)symbol;
    }
    default: {
      parser_error_back(parser, parser->token_len, value_help);
      return NULL;
    }
  }
}
