#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../support/arena.h"

// Node.type: which struct a node is.
typedef enum NodeType {
  NODE_MACHINE,     // MachineNode
  NODE_STATE,       // StateNode
  NODE_TRANSITION,  // TransitionNode
  NODE_IMPORT,      // ImportNode
  NODE_IMPORT_SPECIFIER, // ImportSpecifier
  NODE_ASSIGNMENT,  // Assignment
  NODE_INVOKE,      // InvokeNode
  NODE_LOCAL,       // LocalNode
} NodeType;

// TransitionNode.type.
typedef enum TransitionType {
  TRANSITION_EVENT,      // event => target
  TRANSITION_IMMEDIATE,  // => target, taken as soon as the state is entered
  TRANSITION_DELAY,      // delay(...) => target
} TransitionType;

// Assignment.binding_type: `action name = ...` or `guard name = ...`.
typedef enum AssignmentKind {
  ASSIGNMENT_ACTION,
  ASSIGNMENT_GUARD,
} AssignmentKind;

// Expression.type: which struct an expression is.
typedef enum ExpressionType {
  EXPRESSION_ASSIGN,     // AssignExpression
  EXPRESSION_IDENTIFIER, // IdentifierExpression
  EXPRESSION_GUARD,      // GuardExpression
  EXPRESSION_ACTION,     // ActionExpression
  EXPRESSION_DELAY,      // DelayExpression
  EXPRESSION_ON,         // OnExpression
  EXPRESSION_SPAWN,      // SpawnExpression
  EXPRESSION_SEND,       // SendExpression
  EXPRESSION_SYMBOL,     // SymbolExpression
  EXPRESSION_INVOKE,     // InvokeExpression
  EXPRESSION_MEMBER,     // MemberExpression
} ExpressionType;

// LocalNode.key: which of a state's actions a local sets.
typedef enum LocalKind {
  LOCAL_NONE,
  LOCAL_ENTRY,  // @entry
  LOCAL_EXIT,   // @exit
} LocalKind;

// MachineNode.flags: what the machine's options have to provide.
#define MACHINE_USES_GUARD (1u << 0)
#define MACHINE_USES_ACTION (1u << 1)
#define MACHINE_USES_ASSIGN (1u << 2)
#define MACHINE_USES_DELAY (1u << 3)
#define MACHINE_USES_SERVICE (1u << 4)

typedef struct Node {
  NodeType type;
  size_t start;
  size_t end;
  size_t line;
  struct Node* parent;
  struct Node* child;
  struct Node* next;
} Node;

typedef struct MachineNode {
  Node node;

  unsigned impl_flags;
  unsigned flags;
  size_t name_start;
  size_t name_end;
  size_t initial_start;
  size_t initial_end;
} MachineNode;

typedef struct StateNode {
  Node node;
  // Where the name is in the source. These were ints, which offsets past
  // 2 GB don't fit, and the printer copies the name out with them.
  size_t name_start;
  size_t name_end;

  bool final;
  struct LocalNode* entry;
  struct LocalNode* exit;
  struct TransitionNode* event_transition;
  struct TransitionNode* immediate_transition;
  struct TransitionNode* delay_transition;
  struct InvokeNode* invoke;
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

  TransitionType type;
  struct Expression* event;
  TransitionDelay* delay;

  struct Expression* dest;
  TransitionAction* action;
  TransitionGuard* guard;
  struct TransitionNode* next;
  struct TransitionNode* link;
} TransitionNode;

typedef struct LocalNode {
  Node node;
  LocalKind key;
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
  struct InvokeExpression* expr;
  struct TransitionNode* event_transition;
} InvokeNode;

typedef struct Expression {
  ExpressionType type;
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
  Expression* ref;
} GuardExpression;

typedef struct ActionExpression {
  Expression expression;
  Expression* ref;
} ActionExpression;

typedef struct DelayExpression {
  Expression expression;
  int time;
  Expression* ref;
} DelayExpression;

typedef struct OnExpression {
  Expression expression;
  char* name;
} OnExpression;

typedef struct SpawnExpression {
  Expression expression;
  Expression* target;
} SpawnExpression;

typedef struct SendExpression {
  Expression expression;
  char* actor;
  char* event;
} SendExpression;

typedef struct SymbolExpression {
  Expression expression;
  char* name;
} SymbolExpression;

typedef struct InvokeExpression {
  Expression expression;
  Expression* ref;
} InvokeExpression;

typedef struct MemberExpression {
  Expression expression;
  char* owner;
  Expression* property;
} MemberExpression;

typedef struct Assignment {
  Node node;
  AssignmentKind binding_type;
  char* binding_name;
  Expression* value;
} Assignment;

/*
 * Every node and expression is allocated from the compile's arena (see
 * arena.h). None is freed on its own: they all go when the compile's arena
 * is destroyed. They come back zeroed, so pointer fields start out NULL
 * and flags start out 0. Strings stored in them (names, keys, paths) are
 * arena strings too, or point into the source.
 */
Node* node_create_type(Arena*, NodeType, size_t);
TransitionNode* node_create_transition(Arena*);
MachineNode* node_create_machine(Arena*);
StateNode* node_create_state(Arena*);
ImportNode* node_create_import_statement(Arena*);
ImportSpecifier* node_create_import_specifier(Arena*, char*);
Assignment* node_create_assignment(Arena*, AssignmentKind);
InvokeNode* node_create_invoke(Arena*);
AssignExpression* node_create_assignexpression(Arena*);
IdentifierExpression* node_create_identifierexpression(Arena*);
SymbolExpression* node_create_symbolexpression(Arena*);
GuardExpression* node_create_guardexpression(Arena*);
ActionExpression* node_create_actionexpression(Arena*);
DelayExpression* node_create_delayexpression(Arena*);
OnExpression* node_create_onexpression(Arena*);
SpawnExpression* node_create_spawnexpression(Arena*);
SendExpression* node_create_sendexpression(Arena*);
InvokeExpression* node_create_invokeexpression(Arena*);
MemberExpression* node_create_memberexpression(Arena*);
LocalNode* node_create_local(Arena*);

TransitionAction* create_transition_action(Arena*);

bool node_machine_is_nested(Node*);

void node_append(Node*, Node*);
void node_after_last(Node*, Node*);

TransitionGuard* node_transition_add_guard(Arena*, TransitionNode*, char*);
TransitionAction* node_transition_add_action(Arena*, TransitionNode*, char*);
TransitionDelay* node_transition_add_delay(Arena*, TransitionNode*, char*, DelayExpression*);
void node_local_add_action(LocalNode*, TransitionAction*);

Node* find_closest_node_of_type(Node*, NodeType);
MachineNode* find_closest_machine_node(Node*);
