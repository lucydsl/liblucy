#pragma once

#include "../ast/node.h"
#include "../support/arena.h"
#include "../support/str_builder.h"
#include "../support/str_map.h"
#include "../support/string_list.h"
#include "js_builder.h"

// TSPrinter.flags: which XState types the .d.ts imports.
#define TS_USES_GUARD  (1u << 0)
#define TS_USES_ACTION (1u << 1)
#define TS_USES_INVOKE (1u << 2)
#define TS_USES_DELAY  (1u << 3)
#define TS_USES_ASSIGN (1u << 4)

// TSPrinter.machine_flags.
#define TS_MACHINE_DEFAULT (1u << 0)

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
  const char* source;

  unsigned machine_flags;
  // The name of the last named machine. Known bug: it is kept for the
  // machines after it, named or not.
  bool has_machine_name;
  SourceSpan machine_name;
  const char* xstate_specifier;
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

TSPrinter* ts_printer_create(Arena*, const char* source);
void ts_printer_destroy(TSPrinter*);
/* The finished .d.ts, which the caller must free(). */
char* ts_printer_dump(TSPrinter*);

void ts_printer_add_event(TSPrinter*, const char*);
void ts_printer_add_data(TSPrinter*, const char*);
void ts_printer_add_assign(TSPrinter*, const char*, const char*, const char*);
void ts_printer_add_guard(TSPrinter*, const char*, const char*);
void ts_printer_add_action(TSPrinter*, const char*, const char*);
void ts_printer_add_invoke(TSPrinter*, const char*);
void ts_printer_add_actor(TSPrinter*, const char*);
void ts_printer_add_delay(TSPrinter*, const char*);
void ts_printer_create_machine(TSPrinter*);
void ts_printer_add_machine_name(TSPrinter*, SourceSpan);
void ts_printer_add_default_machine(TSPrinter*);
void ts_printer_add_state_entry(TSPrinter*, const char*, const char*);
void ts_printer_add_entry_action(TSPrinter*, const char*, const char*);
void ts_printer_figure_out_entry_events(TSPrinter*);
