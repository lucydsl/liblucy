#pragma once

#include "../js_builder.h"
#include "../node.h"
#include "../program.h"
#include "../set.h"
#include "ts_printer.h"

// API flags
#define XS_FLAG_USE_REMOTE 1 << 0
#define XS_FLAG_DTS 1 << 1

// Machine implementation flags
#define XS_HAS_STATE_PROP 1 << 0

typedef struct Ref {
  char* key;
  struct Ref* next;
  Expression* value;
} Ref;

typedef struct PrintState {
  int flags;
  char* source;
  Program* program;
  Ref* guard;
  SimpleSet* guard_names;
  Ref* action;
  SimpleSet* action_names;
  Ref* delay;
  SimpleSet* delay_names;
  Ref* service;
  SimpleSet* service_names;
  SimpleSet* events;
  ts_printer_t* tsprinter;
  char* cur_event_name;
  size_t cur_state_start;
  size_t cur_state_end;
  bool in_entry;
} PrintState;

void xs_destroy_state_refs(PrintState*);
void xs_start_assign_call(JSBuilder*, AssignExpression*);
void xs_end_assign_call(JSBuilder*);
void xs_add_spawn_call(PrintState*, JSBuilder*, SpawnExpression*);
void xs_add_send_call(JSBuilder*, SendExpression*);
bool xs_find_and_add_top_level_machine_name(PrintState*, JSBuilder*, char*);
void xs_add_action_ref(PrintState*, char*, Expression*);
void xs_add_guard_ref(PrintState*, char*, Expression*);
void xs_add_delay_ref(PrintState*, char*, Expression*);
void xs_add_service_ref(PrintState*, char*, Expression*);