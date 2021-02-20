#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "node.h"

Node* node_create_type(unsigned short type, size_t size) {
  Node *node = malloc(size);
  node->type = type;
  node->child = NULL;
  return node;
}

TransitionNode* node_create_transition() {
  Node* node = node_create_type(NODE_TRANSITION_TYPE, sizeof(TransitionNode));
  TransitionNode *tn = (TransitionNode*)node;
  return tn;
}

static TransitionGuard* create_transition_guard() {
  TransitionGuard* guard = malloc(sizeof(*guard));
  return guard;
}

static TransitionAction* create_transition_action() {
  TransitionAction* action = malloc(sizeof(*action));
  return action;
}

MachineNode* node_create_machine() {
  Node* node = node_create_type(NODE_MACHINE_TYPE, sizeof(MachineNode));
  MachineNode *machine_node = (MachineNode*)node;
  return machine_node;
}

StateNode* node_create_state() {
  Node* node = node_create_type(NODE_STATE_TYPE, sizeof(StateNode));
  StateNode* state_node = (StateNode*)node;
  return state_node;
}

ImportNode* node_create_import_statement() {
  Node* node = node_create_type(NODE_IMPORT_TYPE, sizeof(ImportNode));
  ImportNode* import_node = (ImportNode*)node;
  return import_node;
};

ImportSpecifier* node_create_import_specifier(char* imported) {
  ImportSpecifier* specifier = malloc(sizeof *specifier); // This is a Node
  specifier->imported = imported;
  return specifier;
}

Assignment* node_create_assignment(unsigned short type) {
  Node* node = node_create_type(NODE_ASSIGNMENT_TYPE, sizeof(Assignment));
  Assignment* assignment = (Assignment*)node;
  assignment->binding_type = type;
  return assignment;
}

AssignExpression* node_create_assignexpression() {
  AssignExpression* expression = malloc(sizeof *expression);
  ((Expression*)expression)->type = EXPRESSION_ASSIGN;
  return expression;
}

IdentifierExpression* node_create_identifierexpression() {
  IdentifierExpression* expression = malloc(sizeof *expression);
  ((Expression*)expression)->type = EXPRESSION_IDENTIFIER;
  return expression;
}

void node_transition_add_guard(TransitionNode* transition_node, char* name) {
  TransitionGuard* guard = create_transition_guard();
  guard->name = name;

  if(transition_node->guard == NULL) {
    transition_node->guard = guard;
  } else {
    TransitionGuard* cur = transition_node->guard;
    while(cur->next != NULL) {
      cur = cur->next;
    }
    cur->next = guard;
  }
}

void node_transition_add_action(TransitionNode* transition_node, char* name) {
  TransitionAction* action = create_transition_action();
  action->name = name;

  if(transition_node->action == NULL) {
    transition_node->action = action;
  } else {
    TransitionAction* cur = transition_node->action;
    while(cur->next != NULL) {
      cur = cur->next;
    }
    cur->next = action;
  }
}

void node_append(Node* parent, Node* child)
{
  if(parent == NULL) {
    return;
  }

  child->parent = parent;

  if(parent->child == NULL) {
    parent->child = child;
  } else {
    Node* sibling = parent->child;
    while(sibling->next != NULL) {
      sibling = sibling->next;
    }
    sibling->next = child;
  }
}

void node_after(Node* ref, Node* new_sibling)
{
  ref->next = new_sibling;
}

static void node_destroy_transition_guards(TransitionGuard* guard) {
  if(guard != NULL) {
    if(guard->next != NULL) {
      node_destroy_transition_guards(guard->next);
    }

    free(guard);
  }
}

static void node_destroy_transition_actions(TransitionAction* action) {
  if(action != NULL) {
    if(action->next != NULL) {
      node_destroy_transition_actions(action->next);
    }

    free(action);
  }
}

Expression* node_clone_expression(Expression* input) {
  Expression* output;
  switch(input->type) {
    case EXPRESSION_IDENTIFIER: {
      IdentifierExpression *in_id = (IdentifierExpression*)input;
      IdentifierExpression *out_id = node_create_identifierexpression();
      out_id->name = strdup(in_id->name);
      output = (Expression*)out_id;
      break;
    }
    case EXPRESSION_ASSIGN: {
      AssignExpression *in_ae = (AssignExpression*)input;
      AssignExpression *out_ae = node_create_assignexpression();
      out_ae->identifier = strdup(in_ae->identifier);
      out_ae->key = strdup(in_ae->key);
      output = (Expression*)out_ae;
      break;
    }
    default: {
      output = NULL;
    }
  }
  return output;
}

void node_destroy_transition(TransitionNode* transition_node) {
  if(transition_node->guard != NULL) {
    node_destroy_transition_guards(transition_node->guard);
  }

  if(transition_node->action != NULL) {
    node_destroy_transition_actions(transition_node->action);
  }
}

void node_destroy_assignment(Assignment* assignment) {
  Expression *expression = assignment->value;

  switch(expression->type) {
    case EXPRESSION_ASSIGN: {
      AssignExpression *assign_expression = (AssignExpression*)expression;
      free(assign_expression->identifier);
      free(assign_expression->key);
      break;
    }
    case EXPRESSION_IDENTIFIER: {
      IdentifierExpression *identifier_expression = (IdentifierExpression*)expression;
      free(identifier_expression->name);
      break;
    }
  }

  free(expression);
}

void node_destroy(Node* node) {
  if(node == NULL)
    return;

  free(node);
}