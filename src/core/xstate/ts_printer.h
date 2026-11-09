#pragma once

#include "../support/arena.h"
#include "../support/str_map.h"
#include "js_builder.h"
#include "../support/str_set.h"
#include "../support/string_list.h"

// ts_printer_t.flags: which XState types the .d.ts imports.
#define XS_TS_PROGRAM_USES_GUARD (1u << 0)
#define XS_TS_PROGRAM_USES_ACTION (1u << 1)
#define XS_TS_PROGRAM_USES_INVOKE (1u << 2)
#define XS_TS_PROGRAM_USES_DELAY (1u << 3)
#define XS_TS_PROGRAM_USES_ASSIGN (1u << 4)

// ts_printer_t.machine_flags.
#define XS_TS_MACHINE_DEFAULT (1u << 0)

// The events a guard or action runs for, in order and without repeats.
typedef struct xs_executor_t {
  string_list_t events;
  str_set_t events_s;
} xs_executor_t;

typedef struct xs_assign_executor_t {
  string_list_t events;
  str_set_t events_s;
  char* data_prop;
} xs_assign_executor_t;

/*
 * Builds the .d.ts for a compile.
 *
 * Create with ts_printer_create and free with ts_printer_destroy. The
 * printer itself and its tables, lists and executors are allocated from
 * the arena it is given; ts_printer_destroy frees the string builders,
 * which are heap-allocated because they grow.
 */
typedef struct ts_printer_t {
  Arena* arena;
  unsigned flags;
  char* source;

  unsigned machine_flags;
  size_t machine_name_start;
  size_t machine_name_end;
  char* xstate_specifier;
  JSBuilder* buffer;
  str_builder_t* event_names_sb;
  str_builder_t* data_names_sb;
  str_builder_t* machine_def_sb;

  ht* guards;
  ht* actions;
  ht* assigns;
  string_list_t* invokes;
  string_list_t* actors;
  string_list_t* delays;

  ht* state_entries; // Map<state, List<Events>>
  ht* entry_actions; // Map<state, List<Actions>>
} ts_printer_t;

ts_printer_t* ts_printer_create(Arena*, char* source);
void ts_printer_destroy(ts_printer_t*);
/* The finished .d.ts, which the caller must free(). */
char* ts_printer_dump(ts_printer_t*);

void ts_printer_add_event(ts_printer_t*, char*);
void ts_printer_add_data(ts_printer_t*, char*);
void ts_printer_add_assign(ts_printer_t*, char*, char*, char*);
void ts_printer_add_guard(ts_printer_t*, char*, char*);
void ts_printer_add_action(ts_printer_t*, char*, char*);
void ts_printer_add_invoke(ts_printer_t*, char*);
void ts_printer_add_actor(ts_printer_t*, char*);
void ts_printer_add_delay(ts_printer_t*, char*);
void ts_printer_create_machine(ts_printer_t*);
void ts_printer_add_machine_name(ts_printer_t*, size_t, size_t);
void ts_printer_add_state_entry(ts_printer_t*, char*, char*);
void ts_printer_add_entry_action(ts_printer_t*, char*, char*);
void ts_printer_figure_out_entry_events(ts_printer_t*);