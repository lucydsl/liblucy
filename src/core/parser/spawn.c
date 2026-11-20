#include "internal.h"

#include <stdbool.h>
#include <string.h>

static ParseStatus consume_inline_spawn_args(Parser* parser, void* expr, TokenType token, const char* arg, size_t index) {
  (void)index;
  SpawnExpression* spawn_expression = (SpawnExpression*)expr;
  switch(token) {
    case TOKEN_IDENTIFIER: {
      IdentifierExpression* identifier_expression = expression_create_identifier(parser->arena);
      identifier_expression->name = arg;
      spawn_expression->target = (Expression*)identifier_expression;
      break;
    }
    case TOKEN_SYMBOL: {
      SymbolExpression* symbol_expression = expression_create_symbol(parser->arena);
      symbol_expression->name = arg;
      spawn_expression->target = (Expression*)symbol_expression;
      break;
    }
    default: {
      break;
    }
  }
  return PARSE_OK;
}

static const char* const value_help =
  "A value is event.data, context.name, self, parent, or a function: one from use, or a :symbol.";

// The value of a field, token being its first token: a reference, never
// an expression. NULL, after an error, for anything else.
static Expression* consume_value(Parser* parser, TokenType token) {
  switch(token) {
    case TOKEN_MEMBER_EXPRESSION: {
      MemberExpression* member = expression_create_member(parser->arena);
      member->owner = parser_take_word(parser);
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
      identifier->name = parser_take_word(parser);
      return (Expression*)identifier;
    }
    case TOKEN_SYMBOL: {
      SymbolExpression* symbol = expression_create_symbol(parser->arena);
      symbol->name = parser_take_word(parser);
      return (Expression*)symbol;
    }
    default: {
      parser_error_back(parser, parser->token_len, value_help);
      return NULL;
    }
  }
}

static bool has_field(const SpawnExpression* spawn_expression, const char* name) {
  for(const SpawnField* field = spawn_expression->fields; field != NULL; field = field->next) {
    if(strcmp(field->name, name) == 0) {
      return true;
    }
  }
  return false;
}

// The block after a spawn, from after its {: a `name = value` on each
// line, up to the }.
static ParseStatus consume_fields(Parser* parser, SpawnExpression* spawn_expression) {
  ParseStatus status = PARSE_OK;
  SpawnField** tail = &spawn_expression->fields;
  for(;;) {
    TokenType token = parser_next_token(parser);
    if(token == TOKEN_EOL) {
      continue;
    }
    if(token == TOKEN_END_BLOCK) {
      return status;
    }
    if(token != TOKEN_IDENTIFIER) {
      if(token == TOKEN_EOF) {
        parser_error(parser, "Expected a } to end the inputs of this spawn.");
      } else {
        parser_error_back(parser, parser->token_len, "Expected the name of an input, as in name = value.");
      }
      return PARSE_FATAL;
    }
    const char* name = parser_take_word(parser);
    if(has_field(spawn_expression, name)) {
      parser_error_backf(parser, parser->token_len, "The input %s is already given.", name);
      status = PARSE_ERROR;
    }
    token = parser_next_token(parser);
    if(token != TOKEN_ASSIGNMENT) {
      parser_error_backf(parser, parser->token_len, "Expected an = after the input's name, as in %s = value.", name);
      return PARSE_FATAL;
    }
    Expression* value = consume_value(parser, parser_next_token(parser));
    if(value == NULL) {
      return PARSE_FATAL;
    }
    SpawnField* field = arena_alloc(parser->arena, sizeof(SpawnField));
    field->name = name;
    field->value = value;
    *tail = field;
    tail = &field->next;

    token = parser_next_token(parser);
    if(token == TOKEN_END_BLOCK) {
      return status;
    }
    if(token != TOKEN_EOL) {
      parser_error_back(parser, parser->token_len, "Expected the end of the line after an input: one name = value to a line.");
      return PARSE_FATAL;
    }
  }
}

ParseStatus parser_consume_inline_spawn(Parser* parser, AssignExpression* assign_expression) {
  ParseStatus status = PARSE_OK;

  SpawnExpression* spawn_expression = expression_create_spawn(parser->arena);
  if(parse_merge(&status, parser_consume_call(parser, "spawn", spawn_expression, &consume_inline_spawn_args)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }
  if(parse_merge(&status, parser_expect_call_argument(parser, "spawn", spawn_expression->target, status)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }
  if(spawn_expression->target == NULL) {
    return status;
  }
  assign_expression->value = (Expression*)spawn_expression;

  // A block of inputs may follow: spawn(todoItem) { title = event.data }
  TokenType token = parser_next_token(parser);
  if(token != TOKEN_BEGIN_BLOCK) {
    parser_rewind(parser, parser->token_len);
    return status;
  }
  if(parse_merge(&status, consume_fields(parser, spawn_expression)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }

  return status;
}
