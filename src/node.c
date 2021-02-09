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