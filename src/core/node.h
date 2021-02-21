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

#define ASSIGNMENT_ACTION 0
#define ASSIGNMENT_GUARD 1

#define EXPRESSION_ASSIGN 0
#define EXPRESSION_IDENTIFIER 1

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

  char* initial;
} MachineNode;

typedef struct StateNode {
  Node node;
  char* name;

  bool final;
} StateNode;

typedef struct TransitionGuard {
  char* name;
  struct TransitionGuard* next;
} TransitionGuard;

typedef struct TransitionAction {
  char* name;
  struct TransitionAction* next;
} TransitionAction;

typedef struct TransitionNode {
  Node node;

  char* event;
  char* dest;
  TransitionGuard* guard;
  TransitionAction* action;
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
  char* identifier;
} AssignExpression;

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
AssignExpression* node_create_assignexpression();
IdentifierExpression* node_create_identifierexpression();

void node_append(Node*, Node*);
void node_after(Node*, Node*);

void node_transition_add_guard(TransitionNode*, char*);
void node_transition_add_action(TransitionNode*, char*);

Expression* node_clone_expression(Expression*);

void node_destroy_assignment(Assignment*);
void node_destroy_import(ImportNode*);
void node_destroy_import_specifier(ImportSpecifier*);
void node_destroy_machine(MachineNode*);
void node_destroy_state(StateNode*);
void node_destroy_transition(TransitionNode*);
void node_destroy_expression(Expression*);
void node_destroy(Node*);