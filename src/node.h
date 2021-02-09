#ifndef LUCY_NODE_H_
#define LUCY_NODE_H_

#define NODE_STATE_TYPE 0
#define NODE_TRANSITION_TYPE 1

typedef struct Node {
  int type;
  char* name;
  struct Node* parent;
} Node;

typedef struct TransitionNode {
  Node super;

  char* event;
  char* dest;
} TransitionNode;

#endif