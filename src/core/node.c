#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "node.h"
#include "node/expression.h"

Node* node_create_type(unsigned short type, size_t size) {
  Node *node = malloc(size);
  node->type = type;
  node->child = NULL;
  node->next = NULL;
  node->parent = NULL;
  return node;
}

TransitionNode* node_create_transition() {
  Node* node = node_create_type(NODE_TRANSITION_TYPE, sizeof(TransitionNode));
  TransitionNode *tn = (TransitionNode*)node;
  tn->type = TRANSITION_EVENT_TYPE;
  tn->event = NULL;
  tn->dest = NULL;
  tn->guard = NULL;
  tn->action = NULL;
  tn->delay = NULL;
  tn->link = NULL;
  tn->next = NULL;
  return tn;
}

static TransitionGuard* create_transition_guard() {
  TransitionGuard* guard = malloc(sizeof(*guard));
  guard->next = NULL;
  guard->expression = NULL;
  return guard;
}

TransitionAction* create_transition_action() {
  TransitionAction* action = malloc(sizeof(*action));
  action->name = NULL;
  action->next = NULL;
  action->expression = NULL;
  return action;
}

static TransitionDelay* create_transition_delay() {
  TransitionDelay* delay = malloc(sizeof(*delay));
  delay->ref = NULL;
  delay->expression = NULL;
  return delay;
}

MachineNode* node_create_machine() {
  Node* node = node_create_type(NODE_MACHINE_TYPE, sizeof(MachineNode));
  MachineNode *machine_node = (MachineNode*)node;
  machine_node->initial = NULL;
  machine_node->name = NULL;
  machine_node->impl_flags = 0;
  machine_node->flags = 0;
  return machine_node;
}

StateNode* node_create_state() {
  Node* node = node_create_type(NODE_STATE_TYPE, sizeof(StateNode));
  StateNode* state_node = (StateNode*)node;
  state_node->final = false;
  state_node->entry = NULL;
  state_node->exit = NULL;
  state_node->event_transition = NULL;
  state_node->immediate_transition = NULL;
  state_node->delay_transition = NULL;
  state_node->invoke = NULL;
  return state_node;
}

LocalNode* node_create_local() {
  Node* node = node_create_type(NODE_LOCAL_TYPE, sizeof(LocalNode));
  LocalNode* local_node = (LocalNode*)node;
  local_node->key = 0;
  local_node->action = NULL;
  return local_node;
}

ImportNode* node_create_import_statement() {
  Node* node = node_create_type(NODE_IMPORT_TYPE, sizeof(ImportNode));
  ImportNode* import_node = (ImportNode*)node;
  return import_node;
};

ImportSpecifier* node_create_import_specifier(char* imported) {
  Node* node = node_create_type(NODE_IMPORT_SPECIFIER_TYPE, sizeof(ImportSpecifier));
  ImportSpecifier* specifier = (ImportSpecifier*)node;
  specifier->imported = imported;
  specifier->local = NULL;
  return specifier;
}

Assignment* node_create_assignment(unsigned short type) {
  Node* node = node_create_type(NODE_ASSIGNMENT_TYPE, sizeof(Assignment));
  Assignment* assignment = (Assignment*)node;
  assignment->binding_type = type;
  return assignment;
}

InvokeNode* node_create_invoke() {
  Node* node = node_create_type(NODE_INVOKE_TYPE, sizeof(InvokeNode));
  InvokeNode* invoke_node = (InvokeNode*)node;
  invoke_node->expr = NULL;
  invoke_node->event_transition = NULL;
  return invoke_node;
}

TransitionGuard* node_transition_add_guard(TransitionNode* transition_node, char* name) {
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
  return guard;
}

TransitionAction* node_transition_add_action(TransitionNode* transition_node, char* name) {
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

TransitionDelay* node_transition_add_delay(TransitionNode* transition_node, char* ref, DelayExpression* expression) {
  TransitionDelay* delay = create_transition_delay();
  delay->expression = expression;
  transition_node->delay = delay;
  return delay;
}

bool node_machine_is_nested(Node* node) {
  return node->type == NODE_MACHINE_TYPE &&
    node->parent != NULL &&
    node->parent->type == NODE_STATE_TYPE;
}

void node_append(Node* parent, Node* child) {
  if(parent == NULL) {
    return;
  }

  child->parent = parent;

  if(parent->child == NULL) {
    parent->child = child;
  } else {
    node_after_last(parent->child, child);
  }
}

void node_after_last(Node* ref, Node* node) {
  Node* sibling = ref;
  while(sibling->next != NULL) {
    sibling = sibling->next;
  }
  sibling->next = node;
}

static void node_destroy_transition_guards(TransitionGuard* guard) {
  if(guard != NULL) {
    if(guard->name != NULL) {
      free(guard->name);
    }
    if(guard->expression != NULL) {
      node_destroy_guardexpression(guard->expression);
    }
    if(guard->next != NULL) {
      node_destroy_transition_guards(guard->next);
    }

    free(guard);
  }
}

static void node_destroy_transition_actions(TransitionAction* action) {
  if(action != NULL) {
    if(action->name != NULL) {
      free(action->name);
    }
    if(action->expression != NULL) {
      switch(action->expression->type) {
        case EXPRESSION_ACTION: {
          node_destroy_actionexpression((ActionExpression*)action->expression);
          break;
        }
        case EXPRESSION_ASSIGN: {
          node_destroy_assignexpression((AssignExpression*)action->expression);
          break;
        }
        case EXPRESSION_SEND: {
          node_destroy_sendexpression((SendExpression*)action->expression);
          break;
        }
        default: {
          printf("Unknown expression type: %i.\n", action->expression->type);
        }
      }
    }
    if(action->next != NULL) {
      node_destroy_transition_actions(action->next);
    }

    free(action);
  }
}

static void node_destroy_transition_delay(TransitionDelay* delay) {
  if(delay->ref != NULL) {
    free(delay->ref);
  }
  if(delay->expression != NULL) {
    node_destroy_delayexpression(delay->expression);
    free(delay->expression);
  }
  free(delay);
}

Node* find_closest_node_of_type(Node* node, int type) {
  Node* cur = node;
  while(cur != NULL && cur->type != type) {
    cur = cur->parent;
  }
  return cur;
}

MachineNode* find_closest_machine_node(Node* node) {
  Node* found = find_closest_node_of_type(node, NODE_MACHINE_TYPE);
  if(found != NULL) {
    return (MachineNode*)found;
  }
  return NULL;
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
      if(in_ae->value != NULL) {
        out_ae->value = node_clone_expression(in_ae->value);
      }
      out_ae->key = strdup(in_ae->key);
      output = (Expression*)out_ae;
      break;
    }
    case EXPRESSION_DELAY: {
      DelayExpression* in_de = (DelayExpression*)input;
      DelayExpression* out_de = node_create_delayexpression();
      out_de->time = in_de->time;
      if(in_de->ref != NULL) {
        out_de->ref = node_clone_expression(in_de->ref);
      }
      output = (Expression*)out_de;
      break;
    }
    case EXPRESSION_SPAWN: {
      SpawnExpression* in_se = (SpawnExpression*)input;
      SpawnExpression* out_se = node_create_spawnexpression();
      out_se->target = node_clone_expression(in_se->target);
      output = (Expression*)out_se;
      break;
    }
    case EXPRESSION_SEND: {
      SendExpression* in_se = (SendExpression*)input;
      SendExpression* out_se = node_create_sendexpression();
      out_se->actor = strdup(in_se->actor);
      out_se->event = strdup(in_se->event);
      output = (Expression*)out_se;
      break;
    }
    case EXPRESSION_SYMBOL: {
      SymbolExpression* in_se = (SymbolExpression*)input;
      SymbolExpression* out_se = node_create_symbolexpression();
      out_se->name = strdup(in_se->name);
      output = (Expression*)out_se;
      break;
    }
    case EXPRESSION_INVOKE: {
      InvokeExpression* in_ie = (InvokeExpression*)input;
      InvokeExpression* out_ie = node_create_invokeexpression();
      if(in_ie->ref != NULL) {
        out_ie->ref = node_clone_expression(in_ie->ref);
      }
      output = (Expression*)out_ie;
      break;
    }
    default: {
      fprintf(stderr, "Cannot clone expressions of type %i\n", input->type);
      output = NULL;
    }
  }
  return output;
}

/**
 * Teardown a transition
 */
void node_destroy_transition(TransitionNode* transition_node) {
  if(transition_node->event != NULL) {
    Expression* event = transition_node->event;
    switch(event->type) {
      case EXPRESSION_ON: {
        node_destroy_onexpression((OnExpression*)event);
        break;
      }
      case EXPRESSION_IDENTIFIER: {
        node_destroy_identifierexpression((IdentifierExpression*)event);
        break;
      }
    }
    free(event);
  }

  if(transition_node->guard != NULL) {
    node_destroy_transition_guards(transition_node->guard);
  }

  if(transition_node->action != NULL) {
    node_destroy_transition_actions(transition_node->action);
  }

  if(transition_node->delay != NULL) {
    node_destroy_transition_delay(transition_node->delay);
  }
}

void node_destroy_assignment(Assignment* assignment) {
  Expression *expression = assignment->value;
  if(assignment != NULL) {
    if(assignment->binding_name != NULL) {
      free(assignment->binding_name);
    }
    if(assignment->value != NULL) {
      node_destroy_expression(assignment->value);
    }
  }
}

void node_destroy_import(ImportNode* import_node) {
  free(import_node->from);
}

void node_destroy_import_specifier(ImportSpecifier* specifier) {
  if(specifier->imported != NULL)
    free(specifier->imported);

  if(specifier->local != NULL)
    free(specifier->local);
}

void node_destroy_machine(MachineNode* machine_node) {
  free(machine_node->initial);
}

void node_destroy_state(StateNode* state_node) {
  // TODO nothing to do, remove?
}

void node_destroy_invoke(InvokeNode* invoke_node) {
  node_destroy_invokeexpression(invoke_node->expr);
  free(invoke_node->expr);
}

void node_destroy_local(LocalNode* local_node) {
  if(local_node->action != NULL) {
    node_destroy_transition_actions(local_node->action);
  }
}

void node_destroy(Node* node) {
  if(node == NULL)
    return;

  free(node);
}