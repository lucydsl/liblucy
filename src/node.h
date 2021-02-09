#ifndef LUCY_NODE_H_
#define LUCY_NODE_H_

#include <stddef.h>
#include <stdbool.h>

#define NODE_MACHINE_TYPE 0
#define NODE_STATE_TYPE 1
#define NODE_TRANSITION_TYPE 2
#define NODE_IMPORT_TYPE 3

typedef struct Node {
  unsigned short type;
  struct Node* parent;
  struct Node* child;
  struct Node* next;
} Node;

typedef struct MachineNode {
  Node node;

  char* initial;
} MachineNode;

typedef struct StateNode {
  Node node;
  char* name;

  bool final;
} StateNode;

typedef struct TransitionNode {
  Node node;

  char* event;
  char* dest;
} TransitionNode;

typedef struct ImportSpecifier {
  Node node;

  char* imported;
  char* local;
} ImportSpecifier;

typedef struct ImportNode {
  Node node;

  char* from;
} ImportNode;

Node* node_create_type(unsigned short, size_t);
TransitionNode* node_create_transition();
MachineNode* node_create_machine();
StateNode* node_create_state();
ImportNode* node_create_import_statement();
ImportSpecifier* node_create_import_specifier(char*);

void node_append(Node*, Node*);
void node_after(Node*, Node*);

#endif