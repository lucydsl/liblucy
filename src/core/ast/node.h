#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "../support/arena.h"

// Node.type: which struct a node is.
typedef enum NodeType {
  NODE_MACHINE,          // MachineNode
  NODE_STATE,            // StateNode
  NODE_TRANSITION,       // TransitionNode
  NODE_IMPORT,           // ImportNode
  NODE_IMPORT_SPECIFIER, // ImportSpecifierNode
  NODE_ASSIGNMENT,       // AssignmentNode
  NODE_INVOKE,           // InvokeNode
  NODE_LOCAL,            // LocalNode
  NODE_EVENT,            // EventNode
  NODE_INPUT,            // InputNode
} NodeType;

// TransitionNode.type.
typedef enum TransitionType {
  TRANSITION_EVENT,     // event => target
  TRANSITION_IMMEDIATE, // => target, taken as soon as the state is entered
  TRANSITION_DELAY,     // delay(...) => target
} TransitionType;

// AssignmentNode.binding_type: `action name = ...` or `guard name = ...`.
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
  EXPRESSION_REMOVE,     // RemoveExpression
  EXPRESSION_SYMBOL,     // SymbolExpression
  EXPRESSION_INVOKE,     // InvokeExpression
  EXPRESSION_MEMBER,     // MemberExpression
} ExpressionType;

// LocalNode.key: which of a state's actions a local sets.
typedef enum LocalKind {
  LOCAL_NONE,
  LOCAL_ENTRY, // @entry
  LOCAL_EXIT,  // @exit
} LocalKind;

typedef struct Node {
  NodeType type;
  struct Node* parent;
  struct Node* child;
  struct Node* next;
} Node;

// A stretch of the source: the characters from start up to, but not
// including, end. Names are kept as spans rather than copied.
typedef struct SourceSpan {
  size_t start;
  size_t end;
} SourceSpan;

typedef struct MachineNode {
  Node node;

  // `machine name { ... }`. A machine made of states at the top level of
  // the file has no name.
  bool has_name;
  SourceSpan name;
  // The name of the state marked `initial`, if one is.
  bool has_initial;
  SourceSpan initial;
} MachineNode;

typedef struct StateNode {
  Node node;
  SourceSpan name;

  bool final;
  struct LocalNode* entry;
  struct LocalNode* exit;
  struct TransitionNode* event_transition;
  struct TransitionNode* immediate_transition;
  struct TransitionNode* delay_transition;
  struct InvokeNode* invoke;
} StateNode;

typedef struct TransitionGuard {
  const char* name;
  struct TransitionGuard* next;
  struct GuardExpression* expression;
} TransitionGuard;

typedef struct TransitionAction {
  const char* name;
  struct TransitionAction* next;
  struct Expression* expression;
} TransitionAction;

typedef struct TransitionDelay {
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

typedef struct ImportSpecifierNode {
  Node node;

  const char* imported;
} ImportSpecifierNode;

typedef struct ImportNode {
  Node node;

  const char* from;
} ImportNode;

// `event name` or `event name(Type)`, in a machine's block.
typedef struct EventNode {
  Node node;

  const char* name;
  // The type of the event's data, or NULL when it carries none.
  const char* type;
} EventNode;

// `input name` or `input name(Type)`, in a machine's block: a key of the
// machine's context that whoever creates it has to supply.
typedef struct InputNode {
  Node node;

  const char* name;
  // The type of the value, or NULL when none is given.
  const char* type;
} InputNode;

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
  const char* name;
} IdentifierExpression;

typedef struct AssignExpression {
  Expression expression;
  const char* key;
  Expression* value;
  // append(key, spawn(x)): the key holds a list, which the spawned machine
  // is added to. The value is then a SpawnExpression.
  bool append;
} AssignExpression;

// remove(key, value): stop the machine that value refers to and take it
// out of the list the key holds. The value is event.data or context.key.
typedef struct RemoveExpression {
  Expression expression;
  const char* key;
  Expression* value;
} RemoveExpression;

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
  const char* name;
} OnExpression;

// One line of a spawn's block, `name = value`. The value is a reference,
// as is the data of a send: a MemberExpression (event.data, context.key),
// an IdentifierExpression (self, parent, or a function from `use`) or a
// SymbolExpression.
//
// A line `:name = function` gives one of the machine's :symbols instead,
// which is code and never part of its context. Its value is the function
// itself, not a call of it: an IdentifierExpression (a function from
// `use`) or a SymbolExpression (one of the spawning machine's own).
typedef struct SpawnField {
  const char* name;
  Expression* value;
  // The line gives a :symbol, not an input.
  bool symbol;
  struct SpawnField* next;
} SpawnField;

typedef struct SpawnExpression {
  Expression expression;
  Expression* target;
  // The inputs given to what is spawned, in a block after the call.
  SpawnField* fields;
} SpawnExpression;

typedef struct SendExpression {
  Expression expression;
  const char* actor;
  const char* event;
  // The event's data, a reference as a SpawnField's value is, or NULL to
  // pass on the data of the event being handled.
  Expression* data;
} SendExpression;

typedef struct SymbolExpression {
  Expression expression;
  const char* name;
} SymbolExpression;

typedef struct InvokeExpression {
  Expression expression;
  Expression* ref;
} InvokeExpression;

typedef struct MemberExpression {
  Expression expression;
  const char* owner;
  Expression* property;
} MemberExpression;

typedef struct AssignmentNode {
  Node node;
  AssignmentKind binding_type;
  const char* binding_name;
  Expression* value;
} AssignmentNode;

/*
 * Every node and expression is allocated from the compile's arena (see
 * arena.h). None is freed on its own: they all go when the compile's arena
 * is destroyed. They come back zeroed, so pointer fields start out NULL
 * and flags start out 0. Strings stored in them (names, keys, paths) are
 * arena strings too, or point into the source.
 */
Node* node_create(Arena*, NodeType, size_t);
TransitionNode* node_create_transition(Arena*);
MachineNode* node_create_machine(Arena*);
StateNode* node_create_state(Arena*);
ImportNode* node_create_import(Arena*);
ImportSpecifierNode* node_create_import_specifier(Arena*, const char*);
AssignmentNode* node_create_assignment(Arena*, AssignmentKind);
InvokeNode* node_create_invoke(Arena*);
EventNode* node_create_event(Arena*);
InputNode* node_create_input(Arena*);
AssignExpression* expression_create_assign(Arena*);
IdentifierExpression* expression_create_identifier(Arena*);
SymbolExpression* expression_create_symbol(Arena*);
GuardExpression* expression_create_guard(Arena*);
ActionExpression* expression_create_action(Arena*);
DelayExpression* expression_create_delay(Arena*);
OnExpression* expression_create_on(Arena*);
SpawnExpression* expression_create_spawn(Arena*);
SendExpression* expression_create_send(Arena*);
RemoveExpression* expression_create_remove(Arena*);
InvokeExpression* expression_create_invoke(Arena*);
MemberExpression* expression_create_member(Arena*);
LocalNode* node_create_local(Arena*);

TransitionAction* node_create_transition_action(Arena*);

bool node_is_nested_machine(Node*);

void node_append_child(Node*, Node*);
void node_append_sibling(Node*, Node*);

TransitionGuard* node_transition_add_guard(Arena*, TransitionNode*, const char*);
TransitionAction* node_transition_add_action(Arena*, TransitionNode*, const char*);
TransitionDelay* node_transition_set_delay(Arena*, TransitionNode*, DelayExpression*);
void node_local_add_action(LocalNode*, TransitionAction*);

Node* node_find_ancestor(Node*, NodeType);
MachineNode* node_find_machine(Node*);
