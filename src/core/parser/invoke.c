#include "internal.h"

static ParseStatus consume_invoke_args(Parser* parser, void* expr, TokenType token, char* arg, size_t index) {
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
  MachineNode* machine_node = node_find_machine(parent_node);
  if(invoke_node->expr->ref != NULL &&
    invoke_node->expr->ref->type == EXPRESSION_SYMBOL && machine_node != NULL) {
    machine_node->flags |= MACHINE_USES_SERVICE;
  }

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
        if(parse_merge(&status, parser_consume_transition(parser)) == PARSE_FATAL) {
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