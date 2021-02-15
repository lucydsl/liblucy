#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "node.h"

Node* node_create_type(unsigned short type, size_t size) {
  Node *node = malloc(size);
  node->type = type;
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
  ImportSpecifier* specifier = malloc(sizeof *specifier);
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