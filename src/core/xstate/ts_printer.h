#pragma once

#include "../ht.h"
#include "../js_builder.h"
#include "../string_list.h"
#include "../set.h"

#define XS_TS_PROGRAM_USES_GUARD 1 << 0
#define XS_TS_PROGRAM_USES_ACTION 2 << 0
#define XS_TS_PROGRAM_USES_INVOKE 3 << 0
#define XS_TS_PROGRAM_USES_DELAY 4 << 0

#define XS_TS_MACHINE_DEFAULT 1 << 0

typedef struct xs_executor_t {
  string_list_t* events;
  SimpleSet* events_s;
} xs_executor_t;

typedef struct ts_printer_t {
  int flags;

  int machine_flags;
  char* machine_name;
  char* xstate_specifier;
  JSBuilder* buffer;
  str_builder_t* imp_sb;
  str_builder_t* event_names_sb;
  str_builder_t* data_names_sb;
  str_builder_t* machine_def_sb;

  ht* guards;
  ht* actions;
  string_list_t* invokes;
  string_list_t* actors;
  string_list_t* delays;

  ht* state_entries; // Map<state, List<Events>>
  ht* entry_actions; // Map<state, List<Actions>>
} ts_printer_t;

void ts_printer_init(ts_printer_t*);
void ts_printer_destroy(ts_printer_t*);
char* ts_printer_dump(ts_printer_t*);
ts_printer_t* ts_printer_alloc();

void ts_printer_add_event(ts_printer_t*, char*);
void ts_printer_add_data(ts_printer_t*, char*);
void ts_printer_add_guard(ts_printer_t*, char*, char*);
void ts_printer_add_action(ts_printer_t*, char*, char*);
void ts_printer_add_invoke(ts_printer_t*, char*);
void ts_printer_add_actor(ts_printer_t*, char*);
void ts_printer_add_delay(ts_printer_t*, char*);
void ts_printer_create_machine(ts_printer_t*);
void ts_printer_add_machine_name(ts_printer_t*, char*);
void ts_printer_add_state_entry(ts_printer_t*, char*, char*);
void ts_printer_add_entry_action(ts_printer_t*, char*, char*);
void ts_printer_figure_out_entry_events(ts_printer_t*);