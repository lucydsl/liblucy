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
#include "../support/str_set.h"
#include "../support/string_list.h"
#include "js_builder.h"

typedef struct JSMachine JSMachine;
typedef struct JSState JSState;
typedef struct JSEvent JSEvent;

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

// A :symbol a machine's options have to provide.
typedef struct JSSymbol {
  const char* name;
  // For an assign, the context key it sets.
  const char* key;
  // The events it is used on, or any_event when it is used somewhere
  // without an event of the user's (entry actions, immediate transitions,
  // timers, invoke results).
  StringList events;
  bool any_event;
  // The machine doesn't use it itself: it only gives it to a machine it
  // spawns (`:name = :symbol` in a spawn's block), which calls it with its
  // own context and events.
  bool forwarded;
  struct JSSymbol* next;
} JSSymbol;

// One of the bags in the options object, in the order the symbols were
// first seen.
typedef struct JSSymbolBag {
  JSSymbol* head;
  JSSymbol** tail;
  StrMap* by_name;
} JSSymbolBag;

// A context key that assign(key) sets from the data of declared events,
// with the types of those events' data.
typedef struct JSEventData {
  const char* key;
  StringList types;
  struct JSEventData* next;
} JSEventData;

/*
 * A machine's interface: what whoever creates it has to supply (the
 * :symbols it names, by the option they are in), and what its states
 * handle and store. The factory's parameters, the declarations and the
 * checks on the machines that spawn it are all made from this.
 */
typedef struct JSInterface {
  JSSymbolBag actions;
  JSSymbolBag assigns;
  JSSymbolBag guards;
  JSSymbolBag delays;
  // Both in the services option: functions that return a promise, for an
  // invoke, and ones that return an actor, for a spawn.
  JSSymbolBag services;
  JSSymbolBag actors;
  // The events its states have transitions on, in order.
  StringList events;
  StrSet event_set;
  // The context keys its assigns set, in order.
  StringList keys;
  StrSet key_set;
  JSEventData* event_data;
  // The events the machine makes itself that its states can get.
  bool has_delay;
  bool has_done;
  bool has_error;
  // The events it sends to whatever created it, with send(parent, event),
  // in order.
  StringList parent_events;
  // It uses its parent, to send to or to give to a machine it spawns, so
  // it takes the parent as an option.
  bool takes_parent;
} JSInterface;

// An event a machine declares with `event name` or `event name(Type)`.
struct JSEvent {
  const char* name;
  // The type of its data, or NULL when it carries none.
  const char* type;
  JSEvent* next;
};

// An input a machine declares with `input name` or `input name(Type)`.
typedef struct JSInput {
  const char* name;
  // Its type, or NULL when none is given.
  const char* type;
  struct JSInput* next;
} JSInput;

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
  // It calls code it doesn't own while it handles an event (an action, a
  // guard, an invoke, ...), which may send it another: so it queues the
  // events sent to it meanwhile, and handles them afterwards.
  bool queued;
  // It has transitions that send() doesn't start (a timer's, an invoke's,
  // entering the initial state), which a queued machine does through run().
  bool needs_run;
  // One of those can happen during another transition: an invoked machine
  // finishing as soon as it starts.
  bool run_nests;
  // The events it declares, in order. NULL when it declares none, and
  // then its events are whatever its states handle.
  JSEvent* events;
  // The inputs it declares, in order: keys of its context that whoever
  // creates it supplies. NULL when it declares none.
  JSInput* inputs;
  // What it needs supplied and what it handles.
  JSInterface* interface;
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
  // The names used as the types of events. An import with one of these
  // names is a type, which the JavaScript leaves out.
  StrSet type_names;
  // The names of the output's own variables for the queue: busy, queue and
  // run, or busy2 and so on when the source imports something of that name.
  const char* busy;
  const char* queue;
  const char* run;
  // Likewise the variable for the machine's parent, and the one for a
  // machine in a list.
  const char* parent;
  const char* actor;
  // The machine being printed.
  JSMachine* machine;
  // Printing TypeScript: each factory comes after its types, and what it
  // declares is annotated (printer_any).
  bool ts;
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
/*
 * The type annotation of something a factory declares, when printing
 * TypeScript: ": any", or "" in JavaScript. The factory's own types are
 * those of its declaration (dts.c), which is what callers see; inside it
 * the events and the machine are untyped.
 */
const char* printer_any(const Printer*);
/* The parameter list of a function that takes the event, or nothing. */
const char* printer_event_param(const Printer*, bool has_event);

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
/* The first transition on event in a list of them, or NULL. */
TransitionNode* printer_find_event_transition_in(TransitionNode*, const char* event);

/* event.c */

/* Set machine's events from its `event` declarations. */
void printer_build_events(Printer*, JSMachine*);
/* Set machine's inputs from its `input` declarations. */
void printer_build_inputs(Printer*, JSMachine*);
/* The input machine declares as name, or NULL. */
JSInput* printer_find_input(JSMachine*, const char* name);
/* The event machine declares as name, or NULL. */
JSEvent* printer_find_event(JSMachine*, const char* name);
/*
 * Warn about what machine's states do that its declarations, or those of
 * a machine it spawns, don't allow. Every machine's events must be built.
 */
void printer_check_events(Printer*, JSMachine*);
/* Whether name is used as an event's type, so that an import of it is a
 * type import. */
bool printer_is_type_name(Printer*, const char* name);

/* interface.c */

/* Set machine's interface from its states. Its events must be built. */
void printer_build_interface(Printer*, JSMachine*);
/* Whether the machine names anything its creator has to supply. */
bool printer_interface_needs_options(const JSInterface*);
/*
 * The bags of an interface, by number, and the option each is in. The last
 * two, services and actors, are both in the services option.
 */
#define INTERFACE_BAGS 6
JSSymbolBag* printer_interface_bag(JSInterface*, size_t index);
const char* printer_interface_option(size_t index);
/*
 * Add to each machine's interface the symbols of its own that it gives to
 * the machines it spawns, in the option the spawned machine uses them in.
 * Every machine's interface must be built.
 */
void printer_forward_symbols(Printer*);
/*
 * Warn about the machines of this file that machine spawns or invokes and
 * that need options, which nothing supplies to them. Every machine's
 * interface must be built.
 */
void printer_check_interfaces(Printer*, JSMachine*);

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

/*
 * The expression an action stands for: the value of its `action name =`
 * binding when it is a named action, else its own expression. NULL, after
 * a warning, for a name with no binding.
 */
Expression* printer_action_expression(Printer*, TransitionAction*);
/* An action as a statement. */
void printer_print_action(Printer*, TransitionAction*);
/* Whether an action's statement uses the event. */
bool printer_action_takes_event(Printer*, TransitionAction*);
bool printer_actions_take_event(Printer*, TransitionAction*);
/* The assign an action does if it spawns something (assign(key, spawn(x))),
 * else NULL. */
AssignExpression* printer_action_spawn(Printer*, TransitionAction*);
/* The argument a machine of this file is created with by another: its
 * parent, if it uses it, else nothing. */
void printer_add_child_options(Printer*, JSMachine* child);
/* Whether a spawn's call is printed over several lines: it gives inputs,
 * or symbols the machine it spawns uses. */
bool printer_spawn_is_long(Printer*, SpawnExpression*);
/* Whether the values a spawn gives use the event. */
bool printer_spawn_takes_event(SpawnExpression*);
/* The expression for a delay's length in milliseconds. */
void printer_add_delay_length(Printer*, DelayExpression*);

/* import.c */

/* A `use` line as an import of the values it names. Returns false, having
 * printed nothing, when it only names types. */
bool printer_print_import(Printer*, ImportNode*);

/* dts.c */

/* The .d.ts for the model, which the caller must free(). */
char* printer_print_dts(Printer*);
/* The `use` lines that name types of events, as type imports. Returns
 * whether there were any. */
bool printer_print_type_imports(Printer*, JSBuilder*);
/* A machine's types, and the declaration of its factory. */
void printer_print_machine_types(Printer*, JSBuilder*, JSMachine*);
