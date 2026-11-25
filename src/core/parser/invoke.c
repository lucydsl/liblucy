#include "internal.h"

#include <stdbool.h>

static ParseStatus consume_invoke_args(Parser* parser, void* expr, TokenType token, const char* arg, size_t index) {
  (void)index;
  InvokeExpression* invoke_expression = (InvokeExpression*)expr;

  if(token == TOKEN_IDENTIFIER) {
    IdentifierExpression* identifier_expression = expression_create_identifier(parser->arena);
    identifier_expression->name = arg;
    invoke_expression->ref = (Expression*)identifier_expression;
  } else if(token == TOKEN_SYMBOL) {
    SymbolExpression* symbol_expression = expression_create_symbol(parser->arena);
    symbol_expression->name = arg;
    invoke_expression->ref = (Expression*)symbol_expression;
  } else {
    parser_error_back(parser, parser->token_len, "Unexpected argument to invoke()");
    return PARSE_ERROR;
  }

  return PARSE_OK;
}

// A line of the block that gives the invoked machine an input or a symbol,
// after its name, up to the end of the line. A } is left for the block.
static ParseStatus consume_field(Parser* parser, InvokeNode* invoke_node, bool symbol, const char* name, bool after_equals) {
  ParseStatus status = parser_consume_spawn_field(parser, invoke_node->spawn, symbol, name, after_equals);
  if(status == PARSE_FATAL) {
    return PARSE_FATAL;
  }
  TokenType token = parser_next_token(parser);
  if(token == TOKEN_END_BLOCK) {
    parser_rewind(parser, parser->token_len);
  } else if(token != TOKEN_EOL) {
    parser_error_back(parser, parser->token_len, "Expected the end of the line after an input: one name = value to a line.");
    return PARSE_FATAL;
  }
  return status;
}

ParseStatus parser_consume_invoke(Parser* parser) {
  ParseStatus status = PARSE_OK;

  InvokeNode* invoke_node = node_create_invoke(parser->arena);
  Node* node = (Node*)invoke_node;

  Node* parent_node = parser->node;
  if(parent_node == NULL || parent_node->type != NODE_STATE) {
    parser_error(parser, "Unexpected parent for invoke.");
    return PARSE_FATAL;
  }

  parser_enter_node(parser, node);
  ((StateNode*)parent_node)->invoke = invoke_node;

  invoke_node->expr = expression_create_invoke(parser->arena);
  if(parse_merge(&status, parser_consume_call(parser, "invoke", invoke_node->expr, &consume_invoke_args)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }
  // Without an argument keep going, so the block after it is still parsed.
  if(parse_merge(&status, parser_expect_call_argument(parser, "invoke", invoke_node->expr->ref, status)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }

  invoke_node->spawn = expression_create_spawn(parser->arena);
  invoke_node->spawn->target = invoke_node->expr->ref;

  TokenType token = parser_next_token(parser);

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
      }
      case TOKEN_IDENTIFIER: {
        // `name = value` is an input for a machine that is invoked;
        // anything else is a transition: `done => next`. Look at the
        // token after the name, and put the name back for a transition.
        const char* word = parser->word;
        size_t word_start = parser->word_start;
        size_t word_end = parser->word_end;
        if(parser_next_token(parser) != TOKEN_ASSIGNMENT) {
          parser_rewind(parser, parser->token_len);
          parser_set_word(parser, word, word_start, word_end);
          if(parse_merge(&status, parser_consume_transition(parser)) == PARSE_FATAL) {
            return PARSE_FATAL;
          }
          break;
        }
        if(parse_merge(&status, consume_field(parser, invoke_node, false, word, true)) == PARSE_FATAL) {
          return PARSE_FATAL;
        }
        break;
      }
      case TOKEN_SYMBOL: {
        if(parse_merge(&status, consume_field(parser, invoke_node, true, parser_take_word(parser), false)) == PARSE_FATAL) {
          return PARSE_FATAL;
        }
        break;
      }
      default: {
        parser_error_unexpected_identifier(parser);
        status = PARSE_FATAL;
        goto end;
      }
    }
  }

end: {
  parser_leave_node(parser);
  return status;
}
}
