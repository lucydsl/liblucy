#include <stdlib.h>
#include <stdio.h>
#include "../node.h"

void node_destroy_expression(Expression*);

/* Creation */
AssignExpression* node_create_assignexpression() {
  AssignExpression* expression = malloc(sizeof *expression);
  ((Expression*)expression)->type = EXPRESSION_ASSIGN;
  expression->value = NULL;
  expression->key = NULL;
  return expression;
}

IdentifierExpression* node_create_identifierexpression() {
  IdentifierExpression* expression = malloc(sizeof *expression);
  ((Expression*)expression)->type = EXPRESSION_IDENTIFIER;
  return expression;
}

GuardExpression* node_create_guardexpression() {
  GuardExpression* expression = malloc(sizeof *expression);
  ((Expression*)expression)->type = EXPRESSION_GUARD;
  return expression;
}

ActionExpression* node_create_actionexpression() {
  ActionExpression* expression = malloc(sizeof *expression);
  ((Expression*)expression)->type = EXPRESSION_ACTION;
  return expression;
}

DelayExpression* node_create_delayexpression() {
  DelayExpression* expression = malloc(sizeof *expression);
  ((Expression*)expression)->type = EXPRESSION_DELAY;
  expression->ref = NULL;
  return expression;
}

OnExpression* node_create_onexpression() {
  OnExpression* expression = malloc(sizeof *expression);
  ((Expression*)expression)->type = EXPRESSION_ON;
  expression->name = NULL;
  return expression;
}

SpawnExpression* node_create_spawnexpression() {
  SpawnExpression* expression = malloc(sizeof *expression);
  ((Expression*)expression)->type = EXPRESSION_SPAWN;
  expression->target = NULL;
  return expression;
}

SendExpression* node_create_sendexpression() {
  SendExpression* expression = malloc(sizeof* expression);
  ((Expression*) expression)->type = EXPRESSION_SEND;
  expression->actor = NULL;
  expression->event = NULL;
  return expression;
}

SymbolExpression* node_create_symbolexpression() {
  SymbolExpression* expression = malloc(sizeof *expression);
  ((Expression*)expression)->type = EXPRESSION_SYMBOL;
  expression->name = NULL;
  return expression;
}

InvokeExpression* node_create_invokeexpression() {
  InvokeExpression* expression = malloc(sizeof *expression);
  ((Expression*)expression)->type = EXPRESSION_INVOKE;
  expression->ref = NULL;
  return expression;
}

/* Teardown */
void node_destroy_assignexpression(AssignExpression* expression) {
  if(expression != NULL) {
    if(expression->value != NULL) {
      node_destroy_expression(expression->value);
    }
    if(expression->key != NULL) {
      free(expression->key);
    }
  }
}

void node_destroy_identifierexpression(IdentifierExpression* expression) {
  if(expression != NULL) {
    free(expression->name);
  }
}

void node_destroy_guardexpression(GuardExpression* expression) {
  if(expression != NULL) {
    free(expression->ref);
  }
}

void node_destroy_actionexpression(ActionExpression* expression) {
  if(expression != NULL) {
    free(expression->ref);
  }
}

void node_destroy_delayexpression(DelayExpression* expression) {
  if(expression != NULL) {
    if(expression->ref != NULL) {
      free(expression->ref);
    }
  }
}

void node_destroy_onexpression(OnExpression* expression) {
  if(expression != NULL) {
    free(expression->name);
  }
}

void node_destroy_spawnexpression(SpawnExpression* expression) {
  if(expression != NULL) {
    free(expression->target);
  }
}

void node_destroy_sendexpression(SendExpression* expression) {
  if(expression != NULL) {
    free(expression->actor);
    free(expression->event);
  }
}

void node_destroy_symbolexpression(SymbolExpression* expression) {
  if(expression != NULL) {
    free(expression->name);
  }
}

void node_destroy_invokeexpression(InvokeExpression* expression) {
  if(expression != NULL) {
    if(expression->ref != NULL) {
      switch(expression->ref->type) {
        case EXPRESSION_IDENTIFIER: {
          node_destroy_identifierexpression((IdentifierExpression*)expression->ref);
          break;
        }
        case EXPRESSION_SYMBOL: {
          node_destroy_symbolexpression((SymbolExpression*)expression->ref);
          break;
        }
        default: {
          printf("Unknown expression type for invoke %i\n", expression->ref->type);
          break;
        }
      }
      free(expression->ref);
    }
  }
}

void node_destroy_expression(Expression* expression) {
  switch(expression->type) {
    case EXPRESSION_IDENTIFIER: {
      IdentifierExpression *identifier_expression = (IdentifierExpression*)expression;
      node_destroy_identifierexpression(identifier_expression);
      break;
    }
    case EXPRESSION_ASSIGN: {
      AssignExpression *assign_expression = (AssignExpression*)expression;
      node_destroy_assignexpression(assign_expression);
      break;
    }
  }
  free(expression);
}