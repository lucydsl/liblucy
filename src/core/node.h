#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "set.h"

#define NODE_MACHINE_TYPE 0
#define NODE_STATE_TYPE 1
#define NODE_TRANSITION_TYPE 2
#define NODE_IMPORT_TYPE 3
#define NODE_IMPORT_SPECIFIER_TYPE 4
#define NODE_ASSIGNMENT_TYPE 5
#define NODE_INVOKE_TYPE 6
#define NODE_LOCAL_TYPE 7

#define TRANSITION_EVENT_TYPE 0
#define TRANSITION_IMMEDIATE_TYPE 1
#define TRANSITION_DELAY_TYPE 2

#define ASSIGNMENT_ACTION 0
#define ASSIGNMENT_GUARD 1

#define EXPRESSION_ASSIGN 0
#define EXPRESSION_IDENTIFIER 1
#define EXPRESSION_GUARD 2
#define EXPRESSION_ACTION 3
#define EXPRESSION_DELAY 4
#define EXPRESSION_ON 5
#define EXPRESSION_SPAWN 6
#define EXPRESSION_SEND 7

typedef struct Node {
  unsigned short type;
  size_t start;
  size_t end;
  unsigned short line;
  struct Node* parent;
  struct Node* child;
  struct Node* next;
} Node;

typedef struct MachineNode {
  Node node;

  int impl_flags;
  char* name;
  char* initial;
} MachineNode;

typedef struct StateNode {
  Node node;
  char* name;

  bool final;
  struct LocalNode* entry;
  struct LocalNode* exit;
} StateNode;

typedef struct TransitionGuard {
  char* name;
  struct TransitionGuard* next;
  struct GuardExpression* expression;
} TransitionGuard;

typedef struct TransitionAction {
  char* name;
  struct TransitionAction* next;
  struct Expression* expression;
} TransitionAction;

typedef struct TransitionDelay {
  char* ref;
  struct DelayExpression* expression;
} TransitionDelay;

typedef struct TransitionNode {
  Node node;

  unsigned short type;
  struct Expression* event;
  TransitionDelay* delay;

  char* dest;
  TransitionAction* action;
  TransitionGuard* guard;
} TransitionNode;

typedef struct LocalNode {
  Node node;
  unsigned short key;
  TransitionAction* action;
} LocalNode;

typedef struct ImportSpecifier {
  Node node;

  char* imported;
  char* local;
} ImportSpecifier;

typedef struct ImportNode {
  Node node;

  char* from;
} ImportNode;

typedef struct InvokeNode {
  Node node;

  char* call;
} InvokeNode;

typedef struct Expression {
  unsigned short type;
} Expression;

typedef struct IdentifierExpression {
  Expression expression;
  char* name;
} IdentifierExpression;

typedef struct AssignExpression {
  Expression expression;
  char* key;
  Expression* value;
} AssignExpression;

typedef struct GuardExpression {
  Expression expression;
  char* ref;
} GuardExpression;

typedef struct ActionExpression {
  Expression expression;
  char* ref;
} ActionExpression;

typedef struct DelayExpression {
  Expression expression;
  int time;
  char* ref;
} DelayExpression;

typedef struct OnExpression {
  Expression expression;
  char* name;
} OnExpression;

typedef struct SpawnExpression {
  Expression expression;
  char* target;
} SpawnExpression;

typedef struct SendExpression {
  Expression expression;
  char* actor;
  char* event;
} SendExpression;

typedef struct Assignment {
  Node node;
  unsigned short binding_type;
  char* binding_name;
  Expression* value;
} Assignment;

Node* node_create_type(unsigned short, size_t);
TransitionNode* node_create_transition();
MachineNode* node_create_machine();
StateNode* node_create_state();
ImportNode* node_create_import_statement();
ImportSpecifier* node_create_import_specifier(char*);
Assignment* node_create_assignment(unsigned short);
InvokeNode* node_create_invoke();
AssignExpression* node_create_assignexpression();
IdentifierExpression* node_create_identifierexpression();
GuardExpression* node_create_guardexpression();
ActionExpression* node_create_actionexpression();
DelayExpression* node_create_delayexpression();
OnExpression* node_create_onexpression();
SpawnExpression* node_create_spawnexpression();
SendExpression* node_create_sendexpression();
LocalNode* node_create_local();

TransitionAction* create_transition_action();

bool node_machine_is_nested(Node*);
bool node_transition_has_sibling_always(TransitionNode*);

void node_append(Node*, Node*);
void node_after_last(Node*, Node*);

TransitionGuard* node_transition_add_guard(TransitionNode*, char*);
TransitionAction* node_transition_add_action(TransitionNode*, char*);
TransitionDelay* node_transition_add_delay(TransitionNode*, char*, DelayExpression*);
void node_local_add_action(LocalNode*, TransitionAction*);

Expression* node_clone_expression(Expression*);

void node_destroy_assignment(Assignment*);
void node_destroy_import(ImportNode*);
void node_destroy_import_specifier(ImportSpecifier*);
void node_destroy_machine(MachineNode*);
void node_destroy_state(StateNode*);
void node_destroy_transition(TransitionNode*);
void node_destroy_expression(Expression*);
void node_destroy_invoke(InvokeNode*);
void node_destroy_local(LocalNode*);
void node_destroy(Node*);