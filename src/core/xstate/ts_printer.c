#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "../str_builder.h"
#include "../set.h"
#include "ts_printer.h"

static void add_str_union(str_builder_t* sb, char* value) {
  int len = str_builder_len(sb);
  char c = str_builder_char_at(sb, len - 1);
  if(c == '\'') {
    str_builder_add_str(sb, " | ", 3);
  }

  str_builder_add_char(sb, '\'');
  str_builder_add_str(sb, value, 0);
  str_builder_add_char(sb, '\'');
}

static xs_executor_t* executor_alloc() {
  xs_executor_t* executor = malloc(sizeof(*executor));
  executor->events = malloc(sizeof(*executor->events));
  executor->events_s = malloc(sizeof(*executor->events_s));
  return executor;
}

static void executor_init(xs_executor_t* executor) {
  string_list_init(executor->events);
  set_init(executor->events_s);
}

static void executor_destroy(xs_executor_t* executor) {
  string_list_destroy(executor->events);
  free(executor->events);
  set_destroy(executor->events_s);
  free(executor);
}

static void executor_add(xs_executor_t* executor, char* event_name) {
  if(set_contains(executor->events_s, event_name) == SET_FALSE) {
    set_add(executor->events_s, event_name);
    string_list_append(executor->events, event_name);
  }
}

static xs_assign_executor_t* assign_executor_alloc() {
  xs_assign_executor_t* executor = malloc(sizeof(*executor));
  executor->events = malloc(sizeof(*executor->events));
  executor->events_s = malloc(sizeof(*executor->events_s));
  return executor;
}

static void assign_executor_init(xs_assign_executor_t* executor) {
  string_list_init(executor->events);
  set_init(executor->events_s);
  executor->data_prop = NULL;
}

static void assign_executor_destroy(xs_assign_executor_t* executor) {
  string_list_destroy(executor->events);
  free(executor->events);
  set_destroy(executor->events_s);
  free(executor->data_prop);
  executor->data_prop = NULL;
  free(executor);
}

static void assign_executor_add(xs_assign_executor_t* executor, char* event_name) {
  if(set_contains(executor->events_s, event_name) == SET_FALSE) {
    set_add(executor->events_s, event_name);
    string_list_append(executor->events, event_name);
  }
}

void ts_printer_init(ts_printer_t* printer) {
  printer->flags = 0;
  printer->buffer = js_builder_create();
  printer->machine_name = NULL;
  printer->xstate_specifier = NULL;
  printer->event_names_sb = NULL;
  printer->data_names_sb = NULL;
  printer->machine_def_sb = str_builder_create();
  printer->guards = NULL;
  printer->actions = NULL;
  printer->invokes = NULL;
  printer->actors = NULL;
  printer->delays = NULL;
  printer->state_entries = NULL;
  printer->entry_actions = NULL;
}

void ts_printer_destroy(ts_printer_t* printer) {
  js_builder_destroy(printer->buffer);
  if(printer->event_names_sb != NULL) {
    str_builder_destroy(printer->event_names_sb);
  }
  if(printer->data_names_sb != NULL) {
    str_builder_destroy(printer->data_names_sb);
  }
  if(printer->actions != NULL) {
    hti it = ht_iterator(printer->actions);
    while (ht_next(&it)) {
      xs_executor_t* executor = (xs_executor_t*)it.value;
      executor_destroy(executor);
    }
  }
  if(printer->guards != NULL) {
    hti it = ht_iterator(printer->guards);
    while (ht_next(&it)) {
      xs_executor_t* executor = (xs_executor_t*)it.value;
      executor_destroy(executor);
    }
  }
  if(printer->assigns != NULL) {
    hti it = ht_iterator(printer->assigns);
    while (ht_next(&it)) {
      xs_assign_executor_t* executor = (xs_assign_executor_t*)it.value;
      assign_executor_destroy(executor);
    }
  }
  if(printer->invokes != NULL) {
    string_list_destroy(printer->invokes);
    free(printer->invokes);
  }
  if(printer->actors != NULL) {
    string_list_destroy(printer->actors);
    free(printer->actors);
  }
  if(printer->delays != NULL) {
    string_list_destroy(printer->delays);
    free(printer->delays);
  }
  if(printer->state_entries != NULL) {
    ht_destroy(printer->state_entries);
  }
  if(printer->entry_actions != NULL) {
    ht_destroy(printer->entry_actions);
  }

  free(printer);
}

ts_printer_t* ts_printer_alloc() {
  ts_printer_t* printer = malloc(sizeof(*printer));

  return printer;
}

static void build_import_clause(ts_printer_t* printer, str_builder_t* buffer) {
  string_list_t identifiers;
  string_list_init(&identifiers);

  if(printer->flags & XS_TS_PROGRAM_USES_ACTION) {
    string_list_append(&identifiers, "Action");
  }
  if(printer->flags & XS_TS_PROGRAM_USES_DELAY) {
    string_list_append(&identifiers, "DelayConfig");
  }
  if(printer->flags & XS_TS_PROGRAM_USES_GUARD) {
    string_list_append(&identifiers, "ConditionPredicate");
  }
  if(printer->flags & XS_TS_PROGRAM_USES_INVOKE) {
    string_list_append(&identifiers, "InvokeCreator");
  }
  if(printer->flags & XS_TS_PROGRAM_USES_ASSIGN) {
    string_list_append(&identifiers, "PartialAssigner");
  }
  string_list_append(&identifiers, "StateMachine");
  str_builder_add_str(buffer, "import { ", 0);

  int i = 0;
  string_list_iterator_t it = string_list_iterator(&identifiers);
  while(string_list_next(&it)) {
    if(i > 0) {
      str_builder_add_str(buffer, ", ", 0);
    }
    str_builder_add_str(buffer, it.node->value, 0);
    i++;
  }

  str_builder_add_str(buffer, " } from '", 0);
  str_builder_add_str(buffer, printer->xstate_specifier, 0);
  str_builder_add_str(buffer, "';\n", 0);
}

static char* build_extract_clause(string_list_t* event_list) {
  str_builder_t* sb = str_builder_create();
  str_builder_add_str(sb, "Extract<TEvent, { type: ", 0);
  string_list_iterator_t ir = string_list_iterator(event_list);
  while(string_list_next(&ir)) {
    if(ir.index > 0) {
      str_builder_add_str(sb, " | ", 0);
    }

    str_builder_add_char(sb, '\'');
    str_builder_add_str(sb, ir.node->value, 0);
    str_builder_add_char(sb, '\'');
  }
  str_builder_add_str(sb, " }>", 0);
  char* extract_clause = str_builder_dump(sb, NULL);
  str_builder_destroy(sb);
  return extract_clause;
}

static void print_machine_definitions(ts_printer_t* printer) {
  JSBuilder* buffer = printer->buffer;

  if(printer->event_names_sb != NULL) {
    js_builder_add_char(buffer, '\n');
    js_builder_add_str(buffer, str_builder_dump(printer->event_names_sb, NULL));
    js_builder_add_str(buffer, ";\n");
  }

  if(printer->data_names_sb != NULL) {
    js_builder_add_char(buffer, '\n');
    js_builder_add_str(buffer, str_builder_dump(printer->data_names_sb, NULL));
    js_builder_add_str(buffer, ";\n");
  }

  // interface CreateMachineOptions<TContext, TEvent extends EventObject> {
  js_builder_add_str(buffer, "\nexport interface Create");
  js_builder_add_str(buffer, printer->machine_name);

  // Options
  js_builder_add_str(buffer, "Options<");
  js_builder_add_str(buffer, "TContext extends Record<");
  if(printer->data_names_sb == NULL) {
    js_builder_add_str(buffer, "any");
  } else {
    js_builder_add_str(buffer, printer->machine_name);
    js_builder_add_str(buffer, "KnownContextKeys");
  }
  js_builder_add_str(buffer, ", any>, TEvent");
  if(printer->event_names_sb != NULL) {
    js_builder_add_str(buffer, " extends { type: ");
    js_builder_add_str(buffer, printer->machine_name);
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
    SimpleSet* actions_added = malloc(sizeof(*actions_added));
    set_init(actions_added);

    if(printer-> actions != NULL) {
      hti it = ht_iterator(printer->actions);
      while (ht_next(&it)) {
        char* action_name = (char*)it.key;
        js_builder_start_prop(buffer, action_name);
        js_builder_add_str(buffer, "Action<\n");
        js_builder_increase_indent(buffer);
        js_builder_add_indent(buffer);
        js_builder_add_str(buffer, "TContext,\n");
        js_builder_add_indent(buffer);
        xs_executor_t* executor = (xs_executor_t*)it.value;
        if(executor->events->length > 0) {
          char* extract_clause = build_extract_clause(executor->events);
          js_builder_add_str(buffer, "TEvent extends ");
          js_builder_add_str(buffer, extract_clause);
          js_builder_add_str(buffer, " ? ");
          js_builder_add_str(buffer, extract_clause);
          js_builder_add_str(buffer, " : TEvent\n");
        } else {
          js_builder_add_str(buffer, "TEvent\n");
        }
        js_builder_decrease_indent(buffer);
        js_builder_add_indent(buffer);
        js_builder_add_str(buffer, ">");

        set_add(actions_added, action_name);
      }
    }

    if(printer->entry_actions != NULL) {
      hti it = ht_iterator(printer->entry_actions);
      while (ht_next(&it)) {
        string_list_t* actions = (string_list_t*)it.value;
        string_list_iterator_t itea = string_list_iterator(actions);
        while(string_list_next(&itea)) {
          char* action_name = itea.node->value;

          if(set_contains(actions_added, action_name) == SET_TRUE) {
            continue;
          }

          js_builder_start_prop(buffer, action_name);
          js_builder_add_str(buffer, "Action<TContext, TEvent>");

          set_add(actions_added, action_name);
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
    hti it = ht_iterator(printer->assigns);
    while (ht_next(&it)) {
      char* cb_name = (char*)it.key;
      js_builder_start_prop(buffer, cb_name);
      js_builder_add_str(buffer, "PartialAssigner<\n");
      js_builder_increase_indent(buffer);
      js_builder_add_indent(buffer);
      js_builder_add_str(buffer, "TContext,\n");
      js_builder_add_indent(buffer);
      xs_assign_executor_t* executor = (xs_assign_executor_t*)it.value;
      if(executor->events->length > 0) {
        char* extract_clause = build_extract_clause(executor->events);
        js_builder_add_str(buffer, "TEvent extends ");
        js_builder_add_str(buffer, extract_clause);
        js_builder_add_str(buffer, " ? ");
        js_builder_add_str(buffer, extract_clause);
        js_builder_add_str(buffer, " : TEvent,\n");
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
    string_list_iterator_t it = string_list_iterator(printer->delays);
    while(string_list_next(&it)) {
      js_builder_start_prop(buffer, it.node->value);
      js_builder_add_str(buffer, "DelayConfig<TContext, TEvent>");
    }
    js_builder_end_object(buffer);
  }
  if(printer->guards != NULL) {
    js_builder_start_prop(buffer, "guards");
    js_builder_start_object(buffer);
    hti it = ht_iterator(printer->guards);
    while (ht_next(&it)) {
      /**
       * Guard<
       *   TContext,
       *   TEvent extends Extract<TEvent, { type: 'GO_BACK' }> ? Extract<TEvent, { type: 'GO_BACK' }> : TEvent
       * >;
       **/
      // Extract<TEvent, { type: 'next' | 'go_back' }>
      
      js_builder_start_prop(buffer, (char*)it.key);
      js_builder_add_str(buffer, "ConditionPredicate<\n");
      js_builder_increase_indent(buffer);
      js_builder_add_indent(buffer);
      js_builder_add_str(buffer, "TContext,\n");
      js_builder_add_indent(buffer);
      xs_executor_t* executor = (xs_executor_t*)it.value;
      if(executor->events->length > 0) {
        char* extract_clause = build_extract_clause(executor->events);
        js_builder_add_str(buffer, "TEvent extends ");
        js_builder_add_str(buffer, extract_clause);
        js_builder_add_str(buffer, " ? ");
        js_builder_add_str(buffer, extract_clause);
        js_builder_add_str(buffer, " : TEvent\n");
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
      string_list_iterator_t ir = string_list_iterator(printer->invokes);
      while(string_list_next(&ir)) {
        char* invoke_name = ir.node->value;
        js_builder_start_prop(buffer, invoke_name);
        js_builder_add_str(buffer, "InvokeCreator<TContext, TEvent>");
      }
    }
    if(printer->actors != NULL) {
      string_list_iterator_t ir = string_list_iterator(printer->actors);
      while(string_list_next(&ir)) {
        char* actor_name = ir.node->value;
        js_builder_start_prop(buffer, actor_name);
        js_builder_add_str(buffer, "StateMachine<any, any, any>");
      }
    }
    js_builder_end_object(buffer);
  }
  js_builder_end_object(buffer);

  // Create machine definition
  js_builder_add_str(buffer, "\n\n");
  js_builder_add_str(buffer, str_builder_dump(printer->machine_def_sb, NULL));
}

char* ts_printer_dump(ts_printer_t* printer) {
  str_builder_t* buffer = str_builder_create();
  build_import_clause(printer, buffer);
  print_machine_definitions(printer);
  str_builder_add_str(buffer, js_builder_dump(printer->buffer), 0);
  
  char* result = str_builder_dump(buffer, NULL);
  str_builder_destroy(buffer);
  return result;
}

void ts_printer_add_event(ts_printer_t* printer, char* event_name) {
  if(printer->event_names_sb == NULL) {
    printer->event_names_sb = str_builder_create();
    str_builder_add_str(printer->event_names_sb, "type ", 0);
    str_builder_add_str(printer->event_names_sb, printer->machine_name, 0);
    str_builder_add_str(printer->event_names_sb, "EventNames = ", 0);
  }

  add_str_union(printer->event_names_sb, event_name);
}

void ts_printer_add_data(ts_printer_t* printer, char* data_prop) {
  if(printer->data_names_sb == NULL) {
    printer->data_names_sb = str_builder_create();
    str_builder_add_str(printer->data_names_sb, "type ", 0);
    str_builder_add_str(printer->data_names_sb, printer->machine_name, 0);
    str_builder_add_str(printer->data_names_sb, "KnownContextKeys = ", 0);
  }

  add_str_union(printer->data_names_sb, data_prop);
}

void ts_printer_add_assign(ts_printer_t* printer, char* data_prop, char* cb_name, char* event_name) {
  // TODO the cb_name should be the key
  if(printer->assigns == NULL) {
    printer->assigns = ht_create();
  }

  xs_assign_executor_t* assign = (xs_assign_executor_t*)ht_get(printer->assigns, cb_name);
  if(assign == NULL) {
    assign = assign_executor_alloc();
    assign_executor_init(assign);
    ht_set(printer->assigns, cb_name, assign);
  }

  // Set the prop
  assign->data_prop = strdup(data_prop);

  // Add an executor for this event.
  if(event_name != NULL) {
    assign_executor_add(assign, event_name);
  }

  printer->flags |= XS_TS_PROGRAM_USES_ASSIGN;
}

void ts_printer_add_guard(ts_printer_t* printer, char* guard_name, char* event_name) {
  if(printer->guards == NULL) {
    printer->guards = ht_create();
  }
  xs_executor_t* guard = (xs_executor_t*)ht_get(printer->guards, guard_name);
  if(guard == NULL) {
    guard = executor_alloc();
    executor_init(guard);
    ht_set(printer->guards, guard_name, guard);
  }
  executor_add(guard, event_name);
  printer->flags |= XS_TS_PROGRAM_USES_GUARD;
}

void ts_printer_add_action(ts_printer_t* printer, char* action_name, char* event_name) {
  if(printer->actions == NULL) {
    printer->actions = ht_create();
  }
  xs_executor_t* action = (xs_executor_t*)ht_get(printer->actions, action_name);
  if(action == NULL) {
    action = executor_alloc();
    executor_init(action);
    ht_set(printer->actions, action_name, action);
  }

  // Add an executor for this event.
  if(event_name != NULL) {
    executor_add(action, event_name);
  }

  printer->flags |= XS_TS_PROGRAM_USES_ACTION;
}

void ts_printer_add_invoke(ts_printer_t* printer, char* invoke_name) {
  if(printer->invokes == NULL) {
    printer->invokes = malloc(sizeof(*printer->invokes));
    string_list_init(printer->invokes);
  }
  string_list_append(printer->invokes, invoke_name);
  printer->flags |= XS_TS_PROGRAM_USES_INVOKE;
}

void ts_printer_add_actor(ts_printer_t* printer, char* name) {
  if(printer->actors == NULL) {
    printer->actors = malloc(sizeof(*printer->actors));
    string_list_init(printer->actors);
  }
  string_list_append(printer->actors, name);
}

void ts_printer_add_delay(ts_printer_t* printer, char* name) {
  if(printer->delays == NULL) {
    printer->delays = malloc(sizeof(*printer->delays));
    string_list_init(printer->delays);
  }
  string_list_append(printer->delays, name);
  printer->flags |= XS_TS_PROGRAM_USES_DELAY;
}

// function createMachine<TContext extends LucyKnownContext<KnownContextKeys>, TEvent extends { type: EventNames } = any>()
void ts_printer_create_machine(ts_printer_t* printer) {
  str_builder_t* sb = printer->machine_def_sb;
  if(printer->machine_flags & XS_TS_MACHINE_DEFAULT) {
    str_builder_add_str(sb, "export default ", 0);
  }

  str_builder_add_str(sb, "function create", 0);
  str_builder_add_str(sb, printer->machine_name, 0);
  str_builder_add_str(sb, "<TContext extends Record<", 0);
  if(printer->data_names_sb == NULL) {
    str_builder_add_str(sb, "any", 0);
  } else {
    str_builder_add_str(sb, printer->machine_name, 0);
    str_builder_add_str(sb, "KnownContextKeys", 0);
  }
  str_builder_add_str(sb, ", any>, TEvent", 0);
  if(printer->event_names_sb != NULL) {
    str_builder_add_str(sb, " extends { type: ", 0);
    str_builder_add_str(sb, printer->machine_name, 0);
    str_builder_add_str(sb, "EventNames } = any", 0);
  }
  str_builder_add_str(sb, ">(options: ", 0);
  str_builder_add_str(sb, "Create", 0);
  str_builder_add_str(sb, printer->machine_name, 0);
  str_builder_add_str(sb, "Options<TContext, TEvent>): StateMachine<TContext, any, TEvent>;", 0);
}

void ts_printer_add_machine_name(ts_printer_t* printer, char* machine_name) {
  if(machine_name == NULL) {
    printer->machine_flags |= XS_TS_MACHINE_DEFAULT;
    printer->machine_name = "Machine";
  } else {
    str_builder_t* sb = str_builder_create();
    int machine_name_len = strlen(machine_name);
    str_builder_add_char(sb, toupper(machine_name[0]));
    int i = 1;
    while(i < machine_name_len) {
      str_builder_add_char(sb, machine_name[i]);
      i++;
    }
    printer->machine_name = str_builder_dump(sb, NULL);
    str_builder_destroy(sb);
  }
}

void ts_printer_add_state_entry(ts_printer_t* printer, char* event_name, char* state_name) {
  if(printer->state_entries == NULL) {
    printer->state_entries = ht_create();
  }
  string_list_t* event_list = (string_list_t*)ht_get(printer->state_entries, state_name);
  if(event_list == NULL) {
    event_list = malloc(sizeof(*event_list));
    string_list_init(event_list);
    ht_set(printer->state_entries, state_name, event_list);
  }

  string_list_append(event_list, event_name);
}

void ts_printer_add_entry_action(ts_printer_t* printer, char* state_name, char* action_name) {
  // Map<state_name, List<ActionName>>
  if(printer->entry_actions == NULL) {
    printer->entry_actions = ht_create();
  }

  string_list_t* actions = (string_list_t*)ht_get(printer->entry_actions, state_name);
  if(actions == NULL) {
    actions = malloc(sizeof(*actions));
    string_list_init(actions);
    ht_set(printer->entry_actions, state_name, actions);
  }

  string_list_append(actions, action_name);
}

void ts_printer_figure_out_entry_events(ts_printer_t* printer) {
  //ht* state_entries; // Map<state_name, List<EventName>>
  //ht* entry_actions; // Map<state_name, List<ActionName>>

  if(printer->state_entries != NULL && printer->entry_actions != NULL) {
    hti it = ht_iterator(printer->state_entries);
    while (ht_next(&it)) {
      const char* state_name = it.key;
      string_list_t* event_list = (string_list_t*)it.value;

      string_list_t* state_entry_actions = ht_get(printer->entry_actions, state_name);
      if(state_entry_actions != NULL) {
        string_list_iterator_t itea = string_list_iterator(state_entry_actions);
        while(string_list_next(&itea)) {
          string_list_iterator_t iti = string_list_iterator(event_list);
          char* action_name = itea.node->value;
          while(string_list_next(&iti)) {
            char* event_name = iti.node->value;
            ts_printer_add_action(printer, action_name, event_name);
          }
        }
      }
    }
  }
}