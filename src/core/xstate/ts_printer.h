#pragma once

#include "../support/arena.h"
#include "../support/str_map.h"
#include "js_builder.h"
#include "../support/str_set.h"
#include "../support/string_list.h"

// TSPrinter.flags: which XState types the .d.ts imports.
#define TS_USES_GUARD (1u << 0)
#define TS_USES_ACTION (1u << 1)
#define TS_USES_INVOKE (1u << 2)
#define TS_USES_DELAY (1u << 3)
#define TS_USES_ASSIGN (1u << 4)

// TSPrinter.machine_flags.
#define TS_MACHINE_DEFAULT (1u << 0)

// The events a guard or action runs for, in order and without repeats.
typedef struct Executor {
  StringList events;
  StrSet events_s;
} Executor;

typedef struct AssignExecutor {
  StringList events;
  StrSet events_s;
  char* data_prop;
} AssignExecutor;

/*
 * Builds the .d.ts for a compile.
 *
 * Create with ts_printer_create and free with ts_printer_destroy. The
 * printer itself and its tables, lists and executors are allocated from
 * the arena it is given; ts_printer_destroy frees the string builders,
 * which are heap-allocated because they grow.
 */
typedef struct TSPrinter {
  Arena* arena;
  unsigned flags;
  char* source;

  unsigned machine_flags;
  size_t machine_name_start;
  size_t machine_name_end;
  char* xstate_specifier;
  JSBuilder* buffer;
  StrBuilder* event_names_sb;
  StrBuilder* data_names_sb;
  StrBuilder* machine_def_sb;

  StrMap* guards;
  StrMap* actions;
  StrMap* assigns;
  StringList* invokes;
  StringList* actors;
  StringList* delays;

  StrMap* state_entries; // Map<state, List<Events>>
  StrMap* entry_actions; // Map<state, List<Actions>>
} TSPrinter;

TSPrinter* ts_printer_create(Arena*, char* source);
void ts_printer_destroy(TSPrinter*);
/* The finished .d.ts, which the caller must free(). */
char* ts_printer_dump(TSPrinter*);

void ts_printer_add_event(TSPrinter*, char*);
void ts_printer_add_data(TSPrinter*, char*);
void ts_printer_add_assign(TSPrinter*, char*, char*, char*);
void ts_printer_add_guard(TSPrinter*, char*, char*);
void ts_printer_add_action(TSPrinter*, char*, char*);
void ts_printer_add_invoke(TSPrinter*, char*);
void ts_printer_add_actor(TSPrinter*, char*);
void ts_printer_add_delay(TSPrinter*, char*);
void ts_printer_create_machine(TSPrinter*);
void ts_printer_add_machine_name(TSPrinter*, size_t, size_t);
void ts_printer_add_state_entry(TSPrinter*, char*, char*);
void ts_printer_add_entry_action(TSPrinter*, char*, char*);
void ts_printer_figure_out_entry_events(TSPrinter*);