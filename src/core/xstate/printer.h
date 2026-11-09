#pragma once

/*
 * The XState printer's internals, shared by the files in xstate/ (apart
 * from js_builder and ts_printer, which have headers of their own).
 */

#include "../diagnostic.h"
#include "../support/arena.h"
#include "js_builder.h"
#include "../ast/node.h"
#include "../ast/program.h"
#include "../support/str_builder.h"
#include "../support/str_set.h"
#include "ts_printer.h"

// MachineNode.printer_flags: what the printer has printed for a machine.
#define MACHINE_PRINTED_STATES (1u << 0)

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
 * The printer's state for one compile. xstate_print owns it, on its stack.
 * Everything it points to is in arena (the AST included), except
 * ts_printer, which xstate_print destroys. ts_printer is NULL unless the
 * .d.ts was asked for.
 */
typedef struct Printer {
  Arena* arena;
  // Where warnings go. The compile's result owns it.
  DiagnosticList* diagnostics;
  char* source;
  Program* program;
  Ref* guard_refs;
  StrSet guard_names;
  Ref* action_refs;
  StrSet action_names;
  Ref* delay_refs;
  StrSet delay_names;
  Ref* service_refs;
  StrSet service_names;
  // The events printed for the state being printed.
  StrSet event_names;
  TSPrinter* ts_printer;
  char* current_event;
  size_t entry_state_start;
  size_t entry_state_end;
  bool in_entry;
} Printer;

void printer_clear_refs(Printer*);
/*
 * Warn that something can't be printed, with text as lucyc prints it, to
 * stream. warnf formats the text as printf does.
 */
void printer_warn(Printer*, LucyStream stream, const char* text);
void printer_warnf(Printer*, LucyStream stream, const char* format, ...)
  STR_BUILDER_PRINTF(3, 4);
void printer_start_assign_call(JSBuilder*, AssignExpression*);
void printer_end_assign_call(JSBuilder*);
void printer_add_spawn_call(Printer*, JSBuilder*, SpawnExpression*);
void printer_add_send_call(JSBuilder*, SendExpression*);
bool printer_add_top_level_machine_name(Printer*, JSBuilder*, char*);
void printer_add_action_ref(Printer*, char*, Expression*);
void printer_add_guard_ref(Printer*, char*, Expression*);
void printer_add_delay_ref(Printer*, char*, Expression*);
void printer_add_service_ref(Printer*, char*, Expression*);

/* machine.c */

void printer_add_machine_binding_name(Printer*, JSBuilder*, MachineNode*);
void printer_enter_machine(Printer*, JSBuilder*, Node*);
void printer_exit_machine(Printer*, JSBuilder*, Node*);

/* printer.c */

void printer_enter_state(Printer*, JSBuilder*, Node*);
void printer_exit_state(Printer*, JSBuilder*, Node*);

/* assignment.c */

void printer_enter_assignment(Printer*, Node*);

/* import.c */

void printer_enter_import(Printer*, JSBuilder*, Node*);

/* invoke.c */

void printer_compile_invoke(Printer*, JSBuilder*, InvokeNode*);

/* transition.c */

void printer_compile_event_transition(Printer*, JSBuilder*, TransitionNode*);
void printer_compile_transition_action(Printer*, JSBuilder*, TransitionAction*, const char*);
void printer_compile_transition_key(Printer*, JSBuilder*, Node*, char*);
void printer_compile_inner_transition(Printer*, JSBuilder*, TransitionNode*);
