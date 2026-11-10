#pragma once

/*
 * The JavaScript printer's internals, shared by the files in js/ (apart
 * from js_builder, which has a header of its own).
 *
 * The printer first builds a model of the program (model.c): a JSMachine
 * for each top-level machine and a JSState for each state, nested ones
 * included, with the names the output uses for them and flags saying what
 * code each needs. The other files print from the model.
 */

#include <stdbool.h>
#include <stddef.h>

#include "../ast/node.h"
#include "../ast/program.h"
#include "../diagnostic.h"
#include "../support/arena.h"
#include "../support/str_builder.h"
#include "../support/str_map.h"
#include "js_builder.h"

typedef struct JSMachine JSMachine;
typedef struct JSState JSState;

// JSState.flags: what the output has for a state.
#define STATE_NEEDS_ENTER       (1u << 0) // an enterX function
#define STATE_NEEDS_EXIT        (1u << 1) // an exitX function
#define STATE_ENTER_TAKES_EVENT (1u << 2) // enterX(event)
#define STATE_EXIT_TAKES_EVENT  (1u << 3) // exitX(event)
#define STATE_NEEDS_ALWAYS      (1u << 4) // an alwaysX function
#define STATE_ENTER_TAKES_CHILD (1u << 5) // enterX(event, enterChild = ...)

// What a state's invoke() runs.
typedef enum InvokeKind {
  INVOKE_NONE,
  INVOKE_PROMISE, // a function returning a promise
  INVOKE_MACHINE, // a machine from this file
} InvokeKind;

struct JSState {
  StateNode* node;
  JSMachine* machine;
  // The compound state this one is nested in, or NULL at the top level.
  JSState* parent;
  JSState* next;
  // The states of the nested machine, if this state has one.
  JSState* child;
  JSState* initial;
  MachineNode* nested;
  const char* name;  // walk
  const char* path;  // red.walk
  const char* ident; // RedWalk, as in enterRedWalk
  const char* var;   // redWalk, as in redWalkTimer
  unsigned flags;
  // A final state of the top-level machine: entering it finishes the
  // machine. Final states of nested machines are just states.
  bool top_final;
  size_t delay_count;
  InvokeKind invoke_kind;
  JSMachine* invoked;
};

struct JSMachine {
  MachineNode* node;
  JSMachine* next;
  // The machine's name, or NULL for the file's unnamed machine.
  const char* name;
  // The exported factory: createLight, or createMachine when unnamed.
  const char* factory;
  JSState* states;
  JSState* initial;
  bool has_final;
};

/*
 * The printer's state for one compile. js_print owns it, on its stack.
 * Everything it points to is in arena (the AST included), except jsb.
 */
typedef struct Printer {
  Arena* arena;
  // Where warnings go. The compile's result owns it.
  DiagnosticList* diagnostics;
  const char* source;
  Program* program;
  JSBuilder* jsb;
  JSMachine* machines;
  // `action name = ...` and `guard name = ...`, by name.
  StrMap* bindings;
  // The machine being printed.
  JSMachine* machine;
} Printer;

/* printer.c */

/* Warn that something can't be printed, with text as lucyc prints it. */
void printer_warn(Printer*, LucyStream stream, const char* text);
void printer_warnf(Printer*, LucyStream stream, const char* format, ...)
  STR_BUILDER_PRINTF(3, 4);
/* The text of a span of the source, as an arena string. */
const char* printer_span(Printer*, SourceSpan);
/* name with its first letter in upper case, as an arena string. */
const char* printer_capitalize(Printer*, const char* name);
/* An arena string of the parts joined. */
const char* printer_concat(Printer*, const char* a, const char* b);

/* model.c */

void printer_build_model(Printer*);
/* The top-level machine called name, or NULL. */
JSMachine* printer_find_machine(Printer*, const char* name);
/*
 * The state a transition from source goes to. Targets name a state of the
 * same machine as source, or a state inside one (`idle.errored`). NULL if
 * there is no such state (printer_build_model warns about those).
 */
JSState* printer_resolve_target(Printer*, JSState* source, Expression* dest);
/* The state that is active when state is entered without saying which of
 * its children to enter: state itself, or its initial child's leaf. */
JSState* printer_leaf(JSState*);
/* Whether a transition's code uses the event. */
bool printer_transition_takes_event(Printer*, JSState* owner, TransitionNode*);
/* Whether entering the target of a transition from owner uses the event. */
bool printer_enter_takes_event(Printer*, JSState* owner, Expression* dest);
/*
 * The state whose enter function (or state assignment) a transition from
 * owner to dest goes through: the target itself, or the state at owner's
 * level the target is nested in, when the way down has entry work. NULL
 * for an unknown target. *target_out gets the target itself.
 */
JSState* printer_enter_point(Printer*, JSState* owner, Expression* dest, JSState** target_out);
/* The name of the event an event transition is on. */
const char* printer_event_name(TransitionNode*);
/* The nearest state above state that has a transition on event, or NULL. */
JSState* printer_find_ancestor_handler(JSState* state, const char* event);
/* A state's first transition on event, or NULL. */
TransitionNode* printer_find_event_transition(JSState*, const char* event);

/* machine.c */

void printer_print_machine(Printer*, JSMachine*);

/* state.c */

void printer_print_states_table(Printer*, JSMachine*);
void printer_print_state_functions(Printer*, JSState*);
/* The variable for a state's index'th timer: greenTimer, greenTimer2. */
const char* printer_timer_name(Printer*, JSState*, size_t index);

/* transition.c */

/*
 * The body of an event handler: every transition on the event, from
 * transition's link chain, as an if/else chain. owner is the state the
 * transitions are in, and has_event says whether `event` is in scope.
 */
void printer_print_event_handler(Printer*, JSState* owner, TransitionNode* transition, bool has_event);
/* One transition's effect: leaving the source, its actions, entering the
 * target. */
void printer_print_transition(Printer*, JSState* owner, TransitionNode*, bool has_event);
/* A state's immediate transitions, as an if chain. */
void printer_print_immediate_transitions(Printer*, JSState* owner, bool has_event);
/* Enter the target of a transition from owner: a `machine.state =` line
 * or a call to the target's enter function. */
void printer_print_enter_target(Printer*, JSState* owner, Expression* dest, bool has_event);
/* The call that leaves owner, if it has an exit function. */
void printer_print_exit_call(Printer*, JSState* owner, bool has_event);
/* The guards of a transition as a JavaScript condition, on the current line. */
void printer_add_guards(Printer*, TransitionGuard*);
/* Whether a handler (a chain of transitions on one event) uses the event. */
bool printer_handler_takes_event(Printer*, JSState* owner, TransitionNode* first);
/* Whether a state's immediate transitions use the event. */
bool printer_immediate_takes_event(Printer*, JSState*);
/* The name of a compound state's handler function for event: redOnTimer. */
const char* printer_handler_name(Printer*, JSState* owner, const char* event);

/* action.c */

/* An action as a statement. */
void printer_print_action(Printer*, TransitionAction*);
/* Whether an action's statement uses the event. */
bool printer_action_takes_event(Printer*, TransitionAction*);
bool printer_actions_take_event(Printer*, TransitionAction*);
/* The assign an action does if it spawns something (assign(key, spawn(x))),
 * else NULL. */
AssignExpression* printer_action_spawn(Printer*, TransitionAction*);
/* The expression that creates and starts what spawn(x) spawns. */
void printer_add_spawn(Printer*, SpawnExpression*);
/* The expression for a delay's length in milliseconds. */
void printer_add_delay_length(Printer*, DelayExpression*);

/* import.c */

void printer_print_import(Printer*, ImportNode*);

/* dts.c */

/* The .d.ts for the model, which the caller must free(). */
char* printer_print_dts(Printer*);
