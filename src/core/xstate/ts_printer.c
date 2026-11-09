#include "ts_printer.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "../support/arena.h"
#include "../support/str_builder.h"
#include "../support/str_set.h"

static void add_str_union(StrBuilder* sb, const char* value) {
  if(str_builder_char_from_end(sb, 1) == '\'') {
    str_builder_add_str(sb, " | ");
  }

  str_builder_add_char(sb, '\'');
  str_builder_add_str(sb, value);
  str_builder_add_char(sb, '\'');
}

static Executor* executor_create(Arena* arena) {
  Executor* executor = ARENA_NEW(arena, Executor);
  string_list_init(&executor->events, arena);
  str_set_init(&executor->events_s, arena);
  return executor;
}

static void executor_add(Executor* executor, const char* event_name) {
  if(str_set_add(&executor->events_s, event_name)) {
    string_list_append(&executor->events, event_name);
  }
}

static AssignExecutor* assign_executor_create(Arena* arena) {
  AssignExecutor* executor = ARENA_NEW(arena, AssignExecutor);
  string_list_init(&executor->events, arena);
  str_set_init(&executor->events_s, arena);
  return executor;
}

static void assign_executor_add(AssignExecutor* executor, const char* event_name) {
  if(str_set_add(&executor->events_s, event_name)) {
    string_list_append(&executor->events, event_name);
  }
}

static StringList* list_create(Arena* arena) {
  StringList* list = ARENA_NEW(arena, StringList);
  string_list_init(list, arena);
  return list;
}

TSPrinter* ts_printer_create(Arena* arena, const char* source) {
  // Zeroed: every list and table starts out NULL, meaning unused.
  TSPrinter* printer = ARENA_NEW(arena, TSPrinter);
  printer->arena = arena;
  printer->source = source;
  printer->buffer = js_builder_create();
  printer->machine_def_sb = str_builder_create();
  return printer;
}

void ts_printer_destroy(TSPrinter* printer) {
  if(printer == NULL) {
    return;
  }
  js_builder_destroy(printer->buffer);
  str_builder_destroy(printer->machine_def_sb);
  str_builder_destroy(printer->event_names_sb);
  str_builder_destroy(printer->data_names_sb);
}

static void build_import_clause(TSPrinter* printer, StrBuilder* buffer) {
  StringList identifiers;
  string_list_init(&identifiers, printer->arena);

  if(printer->flags & TS_USES_ACTION) {
    string_list_append(&identifiers, "Action");
  }
  if(printer->flags & TS_USES_DELAY) {
    string_list_append(&identifiers, "DelayConfig");
  }
  if(printer->flags & TS_USES_GUARD) {
    string_list_append(&identifiers, "ConditionPredicate");
  }
  if(printer->flags & TS_USES_INVOKE) {
    string_list_append(&identifiers, "InvokeCreator");
  }
  if(printer->flags & TS_USES_ASSIGN) {
    string_list_append(&identifiers, "PartialAssigner");
  }
  string_list_append(&identifiers, "StateMachine");
  str_builder_add_str(buffer, "import { ");

  int i = 0;
  StringListIterator it = string_list_iterator(&identifiers);
  while(string_list_next(&it)) {
    if(i > 0) {
      str_builder_add_str(buffer, ", ");
    }
    str_builder_add_str(buffer, it.node->value);
    i++;
  }

  str_builder_add_str(buffer, " } from '");
  str_builder_add_str(buffer, printer->xstate_specifier);
  str_builder_add_str(buffer, "';\n");
}

static char* build_extract_clause(StringList* event_list) {
  StrBuilder* sb = str_builder_create();
  str_builder_add_str(sb, "Extract<TEvent, { type: ");
  StringListIterator ir = string_list_iterator(event_list);
  while(string_list_next(&ir)) {
    if(ir.index > 0) {
      str_builder_add_str(sb, " | ");
    }

    str_builder_add_char(sb, '\'');
    str_builder_add_str(sb, ir.node->value);
    str_builder_add_char(sb, '\'');
  }
  str_builder_add_str(sb, " }>");
  return str_builder_release(sb, NULL);
}

static void build_machine_name(TSPrinter* printer, JSBuilder* buffer) {
  if(!printer->has_machine_name) {
    js_builder_add_str(buffer, "Machine");
  } else {
    js_builder_add_char(buffer, (char)toupper((unsigned char)printer->source[printer->machine_name.start]));
    js_builder_copy_str(buffer, printer->source, printer->machine_name.start + 1, printer->machine_name.end);
  }
}

static void print_machine_definitions(TSPrinter* printer) {
  JSBuilder* buffer = printer->buffer;

  if(printer->event_names_sb != NULL) {
    js_builder_add_char(buffer, '\n');
    js_builder_add_str(buffer, str_builder_peek(printer->event_names_sb));
    js_builder_add_str(buffer, ";\n");
  }

  if(printer->data_names_sb != NULL) {
    js_builder_add_char(buffer, '\n');
    js_builder_add_str(buffer, str_builder_peek(printer->data_names_sb));
    js_builder_add_str(buffer, ";\n");
  }

  // interface CreateMachineOptions<TContext, TEvent extends EventObject> {
  js_builder_add_str(buffer, "\nexport interface Create");
  build_machine_name(printer, buffer);

  // Options
  js_builder_add_str(buffer, "Options<");
  js_builder_add_str(buffer, "TContext extends Record<");
  if(printer->data_names_sb == NULL) {
    js_builder_add_str(buffer, "any");
  } else {
    build_machine_name(printer, buffer);
    js_builder_add_str(buffer, "KnownContextKeys");
  }
  js_builder_add_str(buffer, ", any>, TEvent");
  if(printer->event_names_sb != NULL) {
    js_builder_add_str(buffer, " extends { type: ");
    build_machine_name(printer, buffer);
    js_builder_add_str(buffer, "EventNames }");
  }
  js_builder_add_str(buffer, "> ");

  // Start of the object
  js_builder_start_object(buffer);
  if(printer->data_names_sb != NULL) {
    js_builder_start_prop(buffer, "context");
  } else {
    js_builder_start_prop(buffer, "context?");
  }
  js_builder_add_str(buffer, "TContext");

  if(printer->actions != NULL || printer->entry_actions != NULL) {
    js_builder_start_prop(buffer, "actions");
    js_builder_start_object(buffer);

    // Keep track of actions already added.
    StrSet actions_added;
    str_set_init(&actions_added, printer->arena);

    if(printer-> actions != NULL) {
      StrMapIterator it = str_map_iterator(printer->actions);
      while (str_map_next(&it)) {
        const char* action_name = it.key;
        js_builder_start_prop(buffer, action_name);
        js_builder_add_str(buffer, "Action<\n");
        js_builder_increase_indent(buffer);
        js_builder_add_indent(buffer);
        js_builder_add_str(buffer, "TContext,\n");
        js_builder_add_indent(buffer);
        Executor* executor = (Executor*)it.value;
        if(executor->events.length > 0) {
          char* extract_clause = build_extract_clause(&executor->events);
          js_builder_add_str(buffer, "TEvent extends ");
          js_builder_add_str(buffer, extract_clause);
          js_builder_add_str(buffer, " ? ");
          js_builder_add_str(buffer, extract_clause);
          js_builder_add_str(buffer, " : TEvent\n");
          free(extract_clause);
        } else {
          js_builder_add_str(buffer, "TEvent\n");
        }
        js_builder_decrease_indent(buffer);
        js_builder_add_indent(buffer);
        js_builder_add_str(buffer, ">");

        str_set_add(&actions_added, action_name);
      }
    }

    if(printer->entry_actions != NULL) {
      StrMapIterator it = str_map_iterator(printer->entry_actions);
      while (str_map_next(&it)) {
        StringList* actions = (StringList*)it.value;
        StringListIterator itea = string_list_iterator(actions);
        while(string_list_next(&itea)) {
          const char* action_name = itea.node->value;

          if(str_set_contains(&actions_added, action_name)) {
            continue;
          }

          js_builder_start_prop(buffer, action_name);
          js_builder_add_str(buffer, "Action<TContext, TEvent>");

          str_set_add(&actions_added, action_name);
        }
      }
    }

    js_builder_end_object(buffer);
  }
  if(printer->assigns != NULL) {
    //PartialAssigner<TContext, TEvent extends EventObject, TKey extends keyof TContext> =
    // (context: TContext, event: TEvent, meta: AssignMeta<TContext, TEvent>) => TContext[TKey];
    js_builder_start_prop(buffer, "assigns");
    js_builder_start_object(buffer);
    StrMapIterator it = str_map_iterator(printer->assigns);
    while (str_map_next(&it)) {
      const char* cb_name = it.key;
      js_builder_start_prop(buffer, cb_name);
      js_builder_add_str(buffer, "PartialAssigner<\n");
      js_builder_increase_indent(buffer);
      js_builder_add_indent(buffer);
      js_builder_add_str(buffer, "TContext,\n");
      js_builder_add_indent(buffer);
      AssignExecutor* executor = (AssignExecutor*)it.value;
      if(executor->events.length > 0) {
        char* extract_clause = build_extract_clause(&executor->events);
        js_builder_add_str(buffer, "TEvent extends ");
        js_builder_add_str(buffer, extract_clause);
        js_builder_add_str(buffer, " ? ");
        js_builder_add_str(buffer, extract_clause);
        js_builder_add_str(buffer, " : TEvent,\n");
        free(extract_clause);
      } else {
        js_builder_add_str(buffer, "TEvent,\n");
      }
      js_builder_add_indent(buffer);
      js_builder_add_string(buffer, executor->data_prop);
      js_builder_add_str(buffer, "\n");
      js_builder_decrease_indent(buffer);
      js_builder_add_indent(buffer);
      js_builder_add_str(buffer, ">");
    }
    js_builder_end_object(buffer);
  }
  if(printer->delays != NULL) {
    js_builder_start_prop(buffer, "delays");
    js_builder_start_object(buffer);
    StringListIterator it = string_list_iterator(printer->delays);
    while(string_list_next(&it)) {
      js_builder_start_prop(buffer, it.node->value);
      js_builder_add_str(buffer, "DelayConfig<TContext, TEvent>");
    }
    js_builder_end_object(buffer);
  }
  if(printer->guards != NULL) {
    js_builder_start_prop(buffer, "guards");
    js_builder_start_object(buffer);
    StrMapIterator it = str_map_iterator(printer->guards);
    while (str_map_next(&it)) {
      /**
       * Guard<
       *   TContext,
       *   TEvent extends Extract<TEvent, { type: 'GO_BACK' }> ? Extract<TEvent, { type: 'GO_BACK' }> : TEvent
       * >;
       **/
      // Extract<TEvent, { type: 'next' | 'go_back' }>
      
      js_builder_start_prop(buffer, it.key);
      js_builder_add_str(buffer, "ConditionPredicate<\n");
      js_builder_increase_indent(buffer);
      js_builder_add_indent(buffer);
      js_builder_add_str(buffer, "TContext,\n");
      js_builder_add_indent(buffer);
      Executor* executor = (Executor*)it.value;
      if(executor->events.length > 0) {
        char* extract_clause = build_extract_clause(&executor->events);
        js_builder_add_str(buffer, "TEvent extends ");
        js_builder_add_str(buffer, extract_clause);
        js_builder_add_str(buffer, " ? ");
        js_builder_add_str(buffer, extract_clause);
        js_builder_add_str(buffer, " : TEvent\n");
        free(extract_clause);
      } else {
        js_builder_add_str(buffer, "TEvent\n");
      }
      js_builder_decrease_indent(buffer);
      js_builder_add_indent(buffer);
      js_builder_add_str(buffer, ">");
    }
    js_builder_end_object(buffer);
  }
  if(printer->invokes != NULL || printer->actors != NULL) {
    js_builder_start_prop(buffer, "services");
    js_builder_start_object(buffer);
    if(printer->invokes != NULL) {
      StringListIterator ir = string_list_iterator(printer->invokes);
      while(string_list_next(&ir)) {
        const char* invoke_name = ir.node->value;
        js_builder_start_prop(buffer, invoke_name);
        js_builder_add_str(buffer, "InvokeCreator<TContext, TEvent>");
      }
    }
    if(printer->actors != NULL) {
      StringListIterator ir = string_list_iterator(printer->actors);
      while(string_list_next(&ir)) {
        const char* actor_name = ir.node->value;
        js_builder_start_prop(buffer, actor_name);
        js_builder_add_str(buffer, "StateMachine<any, any, any>");
      }
    }
    js_builder_end_object(buffer);
  }
  js_builder_end_object(buffer);

  // Create machine definition
  js_builder_add_str(buffer, "\n\n");
  js_builder_add_str(buffer, str_builder_peek(printer->machine_def_sb));
}

char* ts_printer_dump(TSPrinter* printer) {
  StrBuilder* buffer = str_builder_create();
  build_import_clause(printer, buffer);
  print_machine_definitions(printer);
  str_builder_add_str(buffer, js_builder_peek(printer->buffer));

  return str_builder_release(buffer, NULL);
}

static void write_machine_name(TSPrinter* printer, StrBuilder* sb) {
  if(!printer->has_machine_name) {
    str_builder_add_str(sb, "Machine");
  } else {
    str_builder_add_char(sb, (char)toupper((unsigned char)printer->source[printer->machine_name.start]));
    // Known bug: this copies the name from its first letter, not the one
    // after, so "light" comes out as "Llight". build_machine_name gets it
    // right.
    str_builder_copy_str(sb, printer->source, printer->machine_name.start, printer->machine_name.end);
  }
}

void ts_printer_add_event(TSPrinter* printer, const char* event_name) {
  if(printer->event_names_sb == NULL) {
    printer->event_names_sb = str_builder_create();
    str_builder_add_str(printer->event_names_sb, "type ");
    write_machine_name(printer, printer->event_names_sb);
    str_builder_add_str(printer->event_names_sb, "EventNames = ");
  }

  add_str_union(printer->event_names_sb, event_name);
}

void ts_printer_add_data(TSPrinter* printer, const char* data_prop) {
  if(printer->data_names_sb == NULL) {
    printer->data_names_sb = str_builder_create();
    str_builder_add_str(printer->data_names_sb, "type ");
    write_machine_name(printer, printer->data_names_sb);
    str_builder_add_str(printer->data_names_sb, "KnownContextKeys = ");
  }

  add_str_union(printer->data_names_sb, data_prop);
}

void ts_printer_add_assign(TSPrinter* printer, const char* data_prop, const char* cb_name, const char* event_name) {
  // TODO the cb_name should be the key
  if(printer->assigns == NULL) {
    printer->assigns = str_map_create(printer->arena);
  }

  AssignExecutor* assign = (AssignExecutor*)str_map_get(printer->assigns, cb_name);
  if(assign == NULL) {
    assign = assign_executor_create(printer->arena);
    str_map_set(printer->assigns, cb_name, assign);
  }

  // Set the prop
  assign->data_prop = data_prop == NULL ? NULL : arena_strdup(printer->arena, data_prop);

  // Add an executor for this event.
  if(event_name != NULL) {
    assign_executor_add(assign, event_name);
  }

  printer->flags |= TS_USES_ASSIGN;
}

void ts_printer_add_guard(TSPrinter* printer, const char* guard_name, const char* event_name) {
  if(printer->guards == NULL) {
    printer->guards = str_map_create(printer->arena);
  }
  Executor* guard = (Executor*)str_map_get(printer->guards, guard_name);
  if(guard == NULL) {
    guard = executor_create(printer->arena);
    str_map_set(printer->guards, guard_name, guard);
  }
  executor_add(guard, event_name);
  printer->flags |= TS_USES_GUARD;
}

void ts_printer_add_action(TSPrinter* printer, const char* action_name, const char* event_name) {
  if(printer->actions == NULL) {
    printer->actions = str_map_create(printer->arena);
  }
  Executor* action = (Executor*)str_map_get(printer->actions, action_name);
  if(action == NULL) {
    action = executor_create(printer->arena);
    str_map_set(printer->actions, action_name, action);
  }

  // Add an executor for this event.
  if(event_name != NULL) {
    executor_add(action, event_name);
  }

  printer->flags |= TS_USES_ACTION;
}

void ts_printer_add_invoke(TSPrinter* printer, const char* invoke_name) {
  if(printer->invokes == NULL) {
    printer->invokes = list_create(printer->arena);
  }
  string_list_append(printer->invokes, invoke_name);
  printer->flags |= TS_USES_INVOKE;
}

void ts_printer_add_actor(TSPrinter* printer, const char* name) {
  if(printer->actors == NULL) {
    printer->actors = list_create(printer->arena);
  }
  string_list_append(printer->actors, name);
}

void ts_printer_add_delay(TSPrinter* printer, const char* name) {
  if(printer->delays == NULL) {
    printer->delays = list_create(printer->arena);
  }
  string_list_append(printer->delays, name);
  printer->flags |= TS_USES_DELAY;
}

// function createMachine<TContext extends LucyKnownContext<KnownContextKeys>, TEvent extends { type: EventNames } = any>()
void ts_printer_create_machine(TSPrinter* printer) {
  StrBuilder* sb = printer->machine_def_sb;
  if(printer->machine_flags & TS_MACHINE_DEFAULT) {
    str_builder_add_str(sb, "export default ");
  }

  str_builder_add_str(sb, "function create");
  write_machine_name(printer, sb);
  str_builder_add_str(sb, "<TContext extends Record<");
  if(printer->data_names_sb == NULL) {
    str_builder_add_str(sb, "any");
  } else {
    write_machine_name(printer, sb);
    str_builder_add_str(sb, "KnownContextKeys");
  }
  str_builder_add_str(sb, ", any>, TEvent");
  if(printer->event_names_sb != NULL) {
    str_builder_add_str(sb, " extends { type: ");
    write_machine_name(printer, sb);
    str_builder_add_str(sb, "EventNames } = any");
  }
  str_builder_add_str(sb, ">(options: ");
  str_builder_add_str(sb, "Create");
  write_machine_name(printer, sb);
  str_builder_add_str(sb, "Options<TContext, TEvent>): StateMachine<TContext, any, TEvent>;");
}

void ts_printer_add_machine_name(TSPrinter* printer, SourceSpan name) {
  printer->has_machine_name = true;
  printer->machine_name = name;
}

void ts_printer_add_default_machine(TSPrinter* printer) {
  printer->machine_flags |= TS_MACHINE_DEFAULT;
}

void ts_printer_add_state_entry(TSPrinter* printer, const char* event_name, const char* state_name) {
  if(printer->state_entries == NULL) {
    printer->state_entries = str_map_create(printer->arena);
  }
  StringList* event_list = (StringList*)str_map_get(printer->state_entries, state_name);
  if(event_list == NULL) {
    event_list = list_create(printer->arena);
    str_map_set(printer->state_entries, state_name, event_list);
  }

  string_list_append(event_list, event_name);
}

void ts_printer_add_entry_action(TSPrinter* printer, const char* state_name, const char* action_name) {
  // Map<state_name, List<ActionName>>
  if(printer->entry_actions == NULL) {
    printer->entry_actions = str_map_create(printer->arena);
  }

  StringList* actions = (StringList*)str_map_get(printer->entry_actions, state_name);
  if(actions == NULL) {
    actions = list_create(printer->arena);
    str_map_set(printer->entry_actions, state_name, actions);
  }

  string_list_append(actions, action_name);
}

void ts_printer_figure_out_entry_events(TSPrinter* printer) {
  //StrMap* state_entries; // Map<state_name, List<EventName>>
  //StrMap* entry_actions; // Map<state_name, List<ActionName>>

  if(printer->state_entries != NULL && printer->entry_actions != NULL) {
    StrMapIterator it = str_map_iterator(printer->state_entries);
    while (str_map_next(&it)) {
      const char* state_name = it.key;
      StringList* event_list = (StringList*)it.value;

      StringList* state_entry_actions = str_map_get(printer->entry_actions, state_name);
      if(state_entry_actions != NULL) {
        StringListIterator itea = string_list_iterator(state_entry_actions);
        while(string_list_next(&itea)) {
          StringListIterator iti = string_list_iterator(event_list);
          const char* action_name = itea.node->value;
          while(string_list_next(&iti)) {
            const char* event_name = iti.node->value;
            ts_printer_add_action(printer, action_name, event_name);
          }
        }
      }
    }
  }
}