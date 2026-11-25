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

static bool has_field(const SpawnExpression* spawn_expression, const char* name, bool symbol) {
  for(const SpawnField* field = spawn_expression->fields; field != NULL; field = field->next) {
    if(field->symbol == symbol && strcmp(field->name, name) == 0) {
      return true;
    }
  }
  return false;
}

// The function a `:name = function` line gives, token being its first
// token: one from `use`, or one of this machine's own :symbols. NULL,
// after an error, for anything else.
static Expression* consume_function(Parser* parser, TokenType token) {
  if(token == TOKEN_IDENTIFIER) {
    IdentifierExpression* identifier = expression_create_identifier(parser->arena);
    identifier->name = parser_take_word(parser);
    return (Expression*)identifier;
  }
  if(token == TOKEN_SYMBOL) {
    SymbolExpression* symbol = expression_create_symbol(parser->arena);
    symbol->name = parser_take_word(parser);
    return (Expression*)symbol;
  }
  parser_error_back(parser, parser->token_len, "A :symbol is given a function: one from use, or a :symbol of this machine.");
  return NULL;
}

// The block after a spawn, from after its {, up to the }. Each line is
// `name = value`, an input, or `:name = function`, one of the machine's
// :symbols.
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
    if(token != TOKEN_IDENTIFIER && token != TOKEN_SYMBOL) {
      if(token == TOKEN_EOF) {
        parser_error(parser, "Expected a } to end the inputs of this spawn.");
      } else {
        parser_error_back(parser, parser->token_len, "Expected the name of an input, as in name = value, or of a symbol, as in :name = function.");
      }
      return PARSE_FATAL;
    }
    bool symbol = token == TOKEN_SYMBOL;
    const char* name = parser_take_word(parser);
    if(has_field(spawn_expression, name, symbol)) {
      parser_error_backf(parser, parser->token_len, symbol ? "The symbol :%s is already given." : "The input %s is already given.", name);
      status = PARSE_ERROR;
    }
    token = parser_next_token(parser);
    if(token != TOKEN_ASSIGNMENT) {
      parser_error_backf(parser, parser->token_len,
        symbol ? "Expected an = after the symbol's name, as in :%s = function." : "Expected an = after the input's name, as in %s = value.", name);
      return PARSE_FATAL;
    }
    token = parser_next_token(parser);
    Expression* value;
    if(symbol) {
      value = consume_function(parser, token);
    } else {
      const char* word = token == TOKEN_MEMBER_EXPRESSION || token == TOKEN_IDENTIFIER || token == TOKEN_SYMBOL ? parser_take_word(parser) : NULL;
      value = parser_consume_value(parser, token, word);
    }
    if(value == NULL) {
      return PARSE_FATAL;
    }
    SpawnField* field = arena_alloc(parser->arena, sizeof(SpawnField));
    field->name = name;
    field->value = value;
    field->symbol = symbol;
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
