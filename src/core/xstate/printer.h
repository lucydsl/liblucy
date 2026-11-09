#pragma once

/*
 * The XState printer's internals, shared by the files in xstate/ (apart
 * from js_builder and ts_printer, which have headers of their own).
 */

#include "../support/arena.h"
#include "js_builder.h"
#include "../ast/node.h"
#include "../ast/program.h"
#include "../support/str_set.h"
#include "ts_printer.h"

// CompileResult.flags and PrintState.flags: the compile's options.
#define XS_FLAG_USE_REMOTE (1u << 0)
#define XS_FLAG_DTS (1u << 1)

// MachineNode.impl_flags: what the printer has printed for a machine.
#define XS_HAS_STATE_PROP (1u << 0)

/*
 * A named guard, action, delay or service that a machine's options
 * object has to list. Refs are allocated from the compile's arena, and
 * key and value point at AST data, which lives just as long.
 */
typedef struct Ref {
  char* key;
  struct Ref* next;
  Expression* value;
} Ref;

/*
 * The printer's state for one compile. compile_xstate owns it, on its
 * stack. Everything it points to is in arena (the AST included), except
 * tsprinter, which compile_xstate destroys.
 */
typedef struct PrintState {
  unsigned flags;
  Arena* arena;
  char* source;
  Program* program;
  Ref* guard;
  str_set_t guard_names;
  Ref* action;
  str_set_t action_names;
  Ref* delay;
  str_set_t delay_names;
  Ref* service;
  str_set_t service_names;
  str_set_t events;
  ts_printer_t* tsprinter;
  char* cur_event_name;
  size_t cur_state_start;
  size_t cur_state_end;
  bool in_entry;
} PrintState;

void xs_clear_state_refs(PrintState*);
void xs_start_assign_call(JSBuilder*, AssignExpression*);
void xs_end_assign_call(JSBuilder*);
void xs_add_spawn_call(PrintState*, JSBuilder*, SpawnExpression*);
void xs_add_send_call(JSBuilder*, SendExpression*);
bool xs_find_and_add_top_level_machine_name(PrintState*, JSBuilder*, char*);
void xs_add_action_ref(PrintState*, char*, Expression*);
void xs_add_guard_ref(PrintState*, char*, Expression*);
void xs_add_delay_ref(PrintState*, char*, Expression*);
void xs_add_service_ref(PrintState*, char*, Expression*);

/* machine.c */

void xs_add_machine_binding_name(PrintState*, JSBuilder*, MachineNode*);
void xs_enter_machine(PrintState*, JSBuilder*, Node*);
void xs_exit_machine(PrintState*, JSBuilder*, Node*);

/* state.c */

void xs_enter_state(PrintState*, JSBuilder*, Node*);
void xs_exit_state(PrintState*, JSBuilder*, Node*);

/* assignment.c */

void xs_enter_assignment(PrintState*, JSBuilder*, Node*);

/* import.c */

void xs_enter_import(PrintState*, JSBuilder*, Node*);

/* invoke.c */

void xs_compile_invoke(PrintState*, JSBuilder*, InvokeNode*);

/* transition.c */

void xs_compile_event_transition(PrintState*, JSBuilder*, TransitionNode*);
void xs_compile_transition_action(PrintState*, JSBuilder*, TransitionAction*, const char*);
void xs_compile_transition_key(PrintState*, JSBuilder*, Node*, char*);
void xs_compile_inner_transition(PrintState*, JSBuilder*, TransitionNode*);
