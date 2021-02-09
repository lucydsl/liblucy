#include <stdlib.h>
#include "node.h"

Node* node_create_type(int type)
{
  Node *node = malloc(sizeof *node);
  node->type = type;
  return node;
}

TransitionNode* node_create_transition()
{
  TransitionNode *node = (TransitionNode*)node_create_type(NODE_TRANSITION_TYPE);
  return node;
}

void node_set_name(Node * node, char * name)
{
  strcpy(node->name, name);
  //node->name = name;
}