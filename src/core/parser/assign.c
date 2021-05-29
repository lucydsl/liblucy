#include "../keyword.h"
#include "../node.h"
#include "../state.h"
#include "core.h"
#include "spawn.h"
#include "token.h"

static inline void maybe_add_machine_uses_assign(AssignExpression* assign_expression, Node* parent_node) {
  Expression* value = assign_expression->value;
  if(value != NULL && value->type == EXPRESSION_SYMBOL) {
    MachineNode* machine_node = find_closest_machine_node(parent_node);
    machine_node->flags |= MACHINE_USES_ASSIGN;
  }
}

/* Public API */
int parser_consume_inline_assign_args(State* state, void* expr, int token, char* arg, int argi) {
  int err = 0;
  AssignExpression* assign_expression = (AssignExpression*)expr;

  if(argi == 0) {
    assign_expression->key = arg;
  } else {
    unsigned short key = keyword_get(arg);
    switch(key) {
      case KW_SPAWN: {
        _check(parser_consume_inline_spawn(state, assign_expression));
        break;
      }
      default: {
        switch(token) {
          case TOKEN_IDENTIFIER: {
            IdentifierExpression* identifier_expression = node_create_identifierexpression();
            identifier_expression->name = arg;
            assign_expression->value = (Expression*)identifier_expression;
            break;
          }
          case TOKEN_SYMBOL: {
            SymbolExpression* symbol_expression = node_create_symbolexpression();
            symbol_expression->name = arg;
            assign_expression->value = (Expression*)symbol_expression;
            break;
          }
        }

        // Not a keyword
        break;
      }
    }
  }
  
  return err;
}

int parser_consume_inline_assign(State* state, Node* node) {
  int err = 0;
  AssignExpression* assign_expression = node_create_assignexpression();

  _check(consume_call_expression(state, "assign", assign_expression, &parser_consume_inline_assign_args));

  switch(node->type) {
    case NODE_TRANSITION_TYPE: {
      TransitionNode* transition_node = (TransitionNode*)node;
      TransitionAction* action = node_transition_add_action(transition_node, NULL);
      action->expression = (Expression*)assign_expression;
      break;
    }
    case NODE_LOCAL_TYPE: {
      LocalNode* local_node = (LocalNode*)node;
      TransitionAction* action = create_transition_action();
      action->expression = (Expression*)assign_expression;
      node_local_add_action(local_node, action);
      break;
    }
  }

  program_add_flag(state->program, PROGRAM_USES_ASSIGN);
  maybe_add_machine_uses_assign(assign_expression, node);

  return err;
}