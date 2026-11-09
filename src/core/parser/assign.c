#include "internal.h"

static inline void maybe_add_machine_uses_assign(AssignExpression* assign_expression, Node* parent_node) {
  Expression* value = assign_expression->value;
  if(value != NULL && value->type == EXPRESSION_SYMBOL) {
    MachineNode* machine_node = node_find_machine(parent_node);
    machine_node->flags |= MACHINE_USES_ASSIGN;
  }
}

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

ParseStatus parser_consume_inline_assign(Parser* parser, Node* node) {
  ParseStatus status = PARSE_OK;
  AssignExpression* assign_expression = expression_create_assign(parser->arena);

  if(parse_merge(&status, parser_consume_call(parser, "assign", assign_expression, &parser_consume_inline_assign_args)) == PARSE_FATAL) {

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

  parser->program->flags |= PROGRAM_USES_ASSIGN;
  maybe_add_machine_uses_assign(assign_expression, node);

  return status;
}