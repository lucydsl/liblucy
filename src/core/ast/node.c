#include "node.h"

#include "../support/arena.h"

Node* node_create(Arena* arena, NodeType type, size_t size) {
  Node* node = arena_alloc(arena, size);
  node->type = type;
  return node;
}

TransitionNode* node_create_transition(Arena* arena) {
  TransitionNode* tn = (TransitionNode*)node_create(arena, NODE_TRANSITION, sizeof(TransitionNode));
  tn->type = TRANSITION_EVENT;
  return tn;
}

TransitionAction* node_create_transition_action(Arena* arena) {
  return ARENA_NEW(arena, TransitionAction);
}

MachineNode* node_create_machine(Arena* arena) {
  return (MachineNode*)node_create(arena, NODE_MACHINE, sizeof(MachineNode));
}

StateNode* node_create_state(Arena* arena) {
  return (StateNode*)node_create(arena, NODE_STATE, sizeof(StateNode));
}

LocalNode* node_create_local(Arena* arena) {
  return (LocalNode*)node_create(arena, NODE_LOCAL, sizeof(LocalNode));
}

ImportNode* node_create_import(Arena* arena) {
  return (ImportNode*)node_create(arena, NODE_IMPORT, sizeof(ImportNode));
}

ImportSpecifierNode* node_create_import_specifier(Arena* arena, const char* imported) {
  ImportSpecifierNode* specifier = (ImportSpecifierNode*)node_create(arena, NODE_IMPORT_SPECIFIER, sizeof(ImportSpecifierNode));
  specifier->imported = imported;
  return specifier;
}

AssignmentNode* node_create_assignment(Arena* arena, AssignmentKind type) {
  AssignmentNode* assignment = (AssignmentNode*)node_create(arena, NODE_ASSIGNMENT, sizeof(AssignmentNode));
  assignment->binding_type = type;
  return assignment;
}

InvokeNode* node_create_invoke(Arena* arena) {
  return (InvokeNode*)node_create(arena, NODE_INVOKE, sizeof(InvokeNode));
}

EventNode* node_create_event(Arena* arena) {
  return (EventNode*)node_create(arena, NODE_EVENT, sizeof(EventNode));
}

TransitionGuard* node_transition_add_guard(Arena* arena, TransitionNode* transition_node, const char* name) {
  TransitionGuard* guard = ARENA_NEW(arena, TransitionGuard);
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
  return guard;
}

TransitionAction* node_transition_add_action(Arena* arena, TransitionNode* transition_node, const char* name) {
  TransitionAction* action = node_create_transition_action(arena);
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
  return action;
}

void node_local_add_action(LocalNode* local_node, TransitionAction* action) {
  if(local_node->action == NULL) {
    local_node->action = action;
  } else {
    TransitionAction* cur = local_node->action;
    while(cur->next != NULL) {
      cur = cur->next;
    }
    cur->next = action;
  }
}

TransitionDelay* node_transition_set_delay(Arena* arena, TransitionNode* transition_node, DelayExpression* expression) {
  TransitionDelay* delay = ARENA_NEW(arena, TransitionDelay);
  delay->expression = expression;
  transition_node->delay = delay;
  return delay;
}

bool node_is_nested_machine(Node* node) {
  return node->type == NODE_MACHINE &&
         node->parent != NULL &&
         node->parent->type == NODE_STATE;
}

void node_append_child(Node* parent, Node* child) {
  if(parent == NULL) {
    return;
  }

  child->parent = parent;

  if(parent->child == NULL) {
    parent->child = child;
  } else {
    node_append_sibling(parent->child, child);
  }
}

void node_append_sibling(Node* ref, Node* node) {
  Node* sibling = ref;
  while(sibling->next != NULL) {
    sibling = sibling->next;
  }
  sibling->next = node;
}

Node* node_find_ancestor(Node* node, NodeType type) {
  Node* cur = node;
  while(cur != NULL && cur->type != type) {
    cur = cur->parent;
  }
  return cur;
}

MachineNode* node_find_machine(Node* node) {
  return (MachineNode*)node_find_ancestor(node, NODE_MACHINE);
}
