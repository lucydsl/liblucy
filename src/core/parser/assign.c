#include "internal.h"

#include <stdbool.h>

/* Public API */
ParseStatus parser_consume_inline_assign_args(Parser* parser, void* expr, TokenType token, const char* arg, size_t index) {
  ParseStatus status = PARSE_OK;
  AssignExpression* assign_expression = (AssignExpression*)expr;

  if(index == 0) {
    assign_expression->key = arg;
  } else {
    Keyword key = keyword_lookup(arg);
    switch(key) {
      case KEYWORD_SPAWN: {
        if(parse_merge(&status, parser_consume_inline_spawn(parser, assign_expression)) == PARSE_FATAL) {
          return PARSE_FATAL;
        }
        break;
      }
      default: {
        switch(token) {
          case TOKEN_IDENTIFIER: {
            IdentifierExpression* identifier_expression = expression_create_identifier(parser->arena);
            identifier_expression->name = arg;
            assign_expression->value = (Expression*)identifier_expression;
            break;
          }
          case TOKEN_SYMBOL: {
            SymbolExpression* symbol_expression = expression_create_symbol(parser->arena);
            symbol_expression->name = arg;
            assign_expression->value = (Expression*)symbol_expression;
            break;
          }
          default: {
            break;
          }
        }

        // Not a keyword
        break;
      }
    }
  }

  return status;
}

ParseStatus parser_consume_append_call(Parser* parser, AssignExpression* expression) {
  ParseStatus status = PARSE_OK;
  expression->append = true;
  if(parse_merge(&status, parser_consume_call(parser, "append", expression, &parser_consume_inline_assign_args)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }
  // Only a spawn can be appended.
  if(status == PARSE_OK && (expression->key == NULL || expression->value == NULL || expression->value->type != EXPRESSION_SPAWN)) {
    parser_error_back(parser, parser->token_len, "append takes the key of a list and a spawn, as in append(todos, spawn(todoItem)).");
    return PARSE_FATAL;
  }
  return status;
}

static ParseStatus consume_inline(Parser* parser, Node* node, bool append) {
  ParseStatus status = PARSE_OK;
  AssignExpression* assign_expression = expression_create_assign(parser->arena);

  ParseStatus call = append ? parser_consume_append_call(parser, assign_expression) : parser_consume_call(parser, "assign", assign_expression, &parser_consume_inline_assign_args);
  if(parse_merge(&status, call) == PARSE_FATAL) {
    return PARSE_FATAL;
  }

  switch(node->type) {
    case NODE_TRANSITION: {
      TransitionNode* transition_node = (TransitionNode*)node;
      TransitionAction* action = node_transition_add_action(parser->arena, transition_node, NULL);
      action->expression = (Expression*)assign_expression;
      break;
    }
    case NODE_LOCAL: {
      LocalNode* local_node = (LocalNode*)node;
      TransitionAction* action = node_create_transition_action(parser->arena);
      action->expression = (Expression*)assign_expression;
      node_local_add_action(local_node, action);
      break;
    }
    default: {
      break;
    }
  }

  return status;
}

ParseStatus parser_consume_inline_assign(Parser* parser, Node* node) {
  return consume_inline(parser, node, false);
}

ParseStatus parser_consume_inline_append(Parser* parser, Node* node) {
  return consume_inline(parser, node, true);
}
