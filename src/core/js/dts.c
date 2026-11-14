#include "printer.h"

#include <stdbool.h>
#include <string.h>

#include "../support/str_set.h"
#include "../support/string_list.h"

/*
 * The .d.ts describes each machine's factory: the options it takes (the
 * context, and the actions, guards, assigns, delays and services it names
 * with :symbols, each typed with the events that can reach it) and the
 * machine it returns.
 *
 * A machine that declares its events (`event set(number)`) has them as
 * types of its own: an interface of them by name, LightEvents, and their
 * union, LightEvent. One that declares none takes the type of its events
 * from the caller, as the TEvent parameter.
 */

// A :symbol the machine's options have to provide.
typedef struct Symbol {
  const char* name;
  // For an assign, the context key it sets.
  const char* key;
  // The events it is used on, or any_event when it is used somewhere
  // without an event of the user's (entry actions, immediate transitions,
  // timers, invoke results).
  StringList events;
  bool any_event;
  struct Symbol* next;
} Symbol;

// One of the bags in the options object, in the order the symbols were
// first seen.
typedef struct Bag {
  Symbol* head;
  Symbol** tail;
  StrMap* by_name;
} Bag;

// A context key that assign(key) sets from the data of declared events,
// with the types of those events' data.
typedef struct EventData {
  const char* key;
  StringList types;
  struct EventData* next;
} EventData;

typedef struct Inventory {
  Printer* printer;
  JSMachine* machine;
  // The prefix of the machine's types: Light, or Machine when unnamed.
  const char* prefix;
  Bag actions;
  Bag assigns;
  Bag guards;
  Bag delays;
  // Promise-returning services, and spawned ones, kept apart as their types
  // differ.
  Bag services;
  Bag actors;
  StringList events;
  StrSet event_set;
  StringList keys;
  StrSet key_set;
  EventData* event_data;
  // The events the machine makes itself that its states can get.
  bool has_delay;
  bool has_done;
  bool has_error;
  // Whether anything is typed with every event, the machine's own included.
  bool uses_any_event;
} Inventory;

// Whether the machine declares its events.
static bool is_typed(const Inventory* inventory) {
  return inventory->machine->events != NULL;
}

static void bag_init(Bag* bag, Arena* arena) {
  bag->head = NULL;
  bag->tail = &bag->head;
  bag->by_name = str_map_create(arena);
}

static void bag_add(Inventory* inventory, Bag* bag, const char* name, const char* key, const char* event) {
  Symbol* symbol = str_map_get(bag->by_name, name);
  if(symbol == NULL) {
    symbol = ARENA_NEW(inventory->printer->arena, Symbol);
    symbol->name = name;
    symbol->key = key;
    string_list_init(&symbol->events, inventory->printer->arena);
    str_map_set(bag->by_name, name, symbol);
    *bag->tail = symbol;
    bag->tail = &symbol->next;
  }
  if(event == NULL) {
    symbol->any_event = true;
  } else {
    bool seen = false;
    for(StringListIterator it = string_list_iterator(&symbol->events); string_list_next(&it);) {
      if(strcmp(it.node->value, event) == 0) {
        seen = true;
      }
    }
    if(!seen) {
      string_list_append(&symbol->events, event);
    }
  }
}

static void add_key(Inventory* inventory, const char* key) {
  if(str_set_add(&inventory->key_set, key)) {
    string_list_append(&inventory->keys, key);
  }
}

static bool list_contains(StringList* list, const char* value) {
  for(StringListIterator it = string_list_iterator(list); string_list_next(&it);) {
    if(strcmp(it.node->value, value) == 0) {
      return true;
    }
  }
  return false;
}

// Note that assign(key) stores the data of event, when the machine
// declares the event with a type.
static void add_event_data(Inventory* inventory, const char* key, const char* event) {
  JSEvent* declared = event != NULL ? printer_find_event(inventory->machine, event) : NULL;
  if(declared == NULL || declared->type == NULL) {
    return;
  }
  EventData** tail = &inventory->event_data;
  while(*tail != NULL && strcmp((*tail)->key, key) != 0) {
    tail = &(*tail)->next;
  }
  if(*tail == NULL) {
    EventData* data = ARENA_NEW(inventory->printer->arena, EventData);
    data->key = key;
    string_list_init(&data->types, inventory->printer->arena);
    *tail = data;
  }
  if(!list_contains(&(*tail)->types, declared->type)) {
    string_list_append(&(*tail)->types, declared->type);
  }
}

static void collect_actions(Inventory* inventory, TransitionAction* action, const char* event) {
  for(; action != NULL; action = action->next) {
    Expression* expression = printer_action_expression(inventory->printer, action);
    if(expression == NULL) {
      continue;
    }
    if(expression->type == EXPRESSION_ASSIGN) {
      AssignExpression* assign = (AssignExpression*)expression;
      add_key(inventory, assign->key);
      if(assign->value == NULL) {
        add_event_data(inventory, assign->key, event);
      } else if(assign->value->type == EXPRESSION_SYMBOL) {
        bag_add(inventory, &inventory->assigns, ((SymbolExpression*)assign->value)->name, assign->key, event);
      } else if(assign->value->type == EXPRESSION_SPAWN) {
        Expression* target = ((SpawnExpression*)assign->value)->target;
        if(target->type == EXPRESSION_SYMBOL) {
          bag_add(inventory, &inventory->actors, ((SymbolExpression*)target)->name, NULL, event);
        }
      }
    } else if(expression->type == EXPRESSION_ACTION) {
      Expression* ref = ((ActionExpression*)expression)->ref;
      if(ref->type == EXPRESSION_SYMBOL) {
        bag_add(inventory, &inventory->actions, ((SymbolExpression*)ref)->name, NULL, event);
      }
    } else if(expression->type == EXPRESSION_SYMBOL) {
      bag_add(inventory, &inventory->actions, ((SymbolExpression*)expression)->name, NULL, event);
    }
  }
}

static void collect_guards(Inventory* inventory, TransitionGuard* guard, const char* event) {
  for(; guard != NULL; guard = guard->next) {
    Expression* ref = NULL;
    if(guard->name != NULL) {
      AssignmentNode* binding = str_map_get(inventory->printer->bindings, guard->name);
      ref = binding != NULL ? binding->value : NULL;
    } else if(guard->expression != NULL) {
      ref = guard->expression->ref;
    }
    if(ref != NULL && ref->type == EXPRESSION_SYMBOL) {
      bag_add(inventory, &inventory->guards, ((SymbolExpression*)ref)->name, NULL, event);
    }
  }
}

static void collect_transitions(Inventory* inventory, TransitionNode* transition, bool named_events) {
  for(; transition != NULL; transition = transition->next) {
    const char* event = named_events ? printer_event_name(transition) : NULL;
    if(named_events && event != NULL && str_set_add(&inventory->event_set, event)) {
      string_list_append(&inventory->events, event);
    }
    collect_guards(inventory, transition->guard, event);
    collect_actions(inventory, transition->action, event);
  }
}

static void collect_states(Inventory* inventory, JSState* state) {
  for(; state != NULL; state = state->next) {
    StateNode* node = state->node;
    if(node->entry != NULL) {
      collect_actions(inventory, node->entry->action, NULL);
    }
    if(node->exit != NULL) {
      collect_actions(inventory, node->exit->action, NULL);
    }
    collect_transitions(inventory, node->event_transition, true);
    collect_transitions(inventory, node->immediate_transition, false);
    if(node->delay_transition != NULL) {
      inventory->has_delay = true;
    }
    for(TransitionNode* t = node->delay_transition; t != NULL; t = t->next) {
      DelayExpression* delay = t->delay != NULL ? t->delay->expression : NULL;
      if(delay != NULL && delay->ref != NULL && delay->ref->type == EXPRESSION_SYMBOL) {
        bag_add(inventory, &inventory->delays, ((SymbolExpression*)delay->ref)->name, NULL, NULL);
      }
    }
    collect_transitions(inventory, node->delay_transition, false);
    if(node->invoke != NULL) {
      inventory->has_done = true;
      for(TransitionNode* t = node->invoke->event_transition; t != NULL; t = t->next) {
        const char* name = printer_event_name(t);
        if(name != NULL && strcmp(name, "error") == 0) {
          inventory->has_error = true;
        }
      }
      Expression* ref = node->invoke->expr != NULL ? node->invoke->expr->ref : NULL;
      if(ref != NULL && ref->type == EXPRESSION_SYMBOL) {
        bag_add(inventory, &inventory->services, ((SymbolExpression*)ref)->name, NULL, NULL);
      }
      collect_transitions(inventory, node->invoke->event_transition, false);
    }
    collect_states(inventory, state->child);
  }
}

// The event type a symbol sees: the events it is used on, or any.
static void add_event_type(Inventory* inventory, JSBuilder* jsb, Symbol* symbol) {
  bool any = symbol->any_event || symbol->events.length == 0;
  if(is_typed(inventory)) {
    if(any) {
      js_builder_add_format(jsb, "%sAnyEvent", inventory->prefix);
      return;
    }
    js_builder_add_format(jsb, "%sEvents[", inventory->prefix);
  } else {
    if(any) {
      js_builder_add_str(jsb, "TEvent");
      return;
    }
    js_builder_add_str(jsb, "Extract<TEvent, { type: ");
  }
  for(StringListIterator it = string_list_iterator(&symbol->events); string_list_next(&it);) {
    js_builder_add_format(jsb, "%s'%s'", it.index > 0 ? " | " : "", it.node->value);
  }
  js_builder_add_str(jsb, is_typed(inventory) ? "]" : " }>");
}

// A bag of the options interface: `actions: { log: (...) => void; };`.
static void print_bag(Inventory* inventory, JSBuilder* jsb, Bag* bag, const char* name, const char* returns) {
  if(bag->head == NULL) {
    return;
  }
  js_builder_open(jsb, "%s: {", name);
  for(Symbol* symbol = bag->head; symbol != NULL; symbol = symbol->next) {
    js_builder_add_indent(jsb);
    js_builder_add_format(jsb, "%s: (", symbol->name);
    js_builder_add_str(jsb, "context: TContext, event: ");
    add_event_type(inventory, jsb, symbol);
    js_builder_add_str(jsb, ") => ");
    if(symbol->key != NULL) {
      js_builder_add_format(jsb, "TContext['%s']", symbol->key);
    } else {
      js_builder_add_str(jsb, returns);
    }
    js_builder_add_str(jsb, ";\n");
  }
  js_builder_close(jsb, "};");
}

static void add_state_names(JSBuilder* jsb, JSState* state, bool* first) {
  for(; state != NULL; state = state->next) {
    if(state->child != NULL) {
      add_state_names(jsb, state->child, first);
    } else {
      js_builder_add_format(jsb, "%s'%s'", *first ? "" : " | ", state->path);
      *first = false;
    }
  }
}

// Whether any symbol in the bag is typed with every event.
static bool bag_uses_any_event(Bag* bag) {
  for(Symbol* symbol = bag->head; symbol != NULL; symbol = symbol->next) {
    if(symbol->any_event || symbol->events.length == 0) {
      return true;
    }
  }
  return false;
}

// The types of a machine's events, when it declares them: the interface
// of them by name, their names, their union, and the union with the
// events the machine makes itself if anything is typed with that.
static void print_event_types(Inventory* inventory, JSBuilder* jsb) {
  const char* prefix = inventory->prefix;
  js_builder_open(jsb, "export interface %sEvents {", prefix);
  for(JSEvent* event = inventory->machine->events; event != NULL; event = event->next) {
    if(event->type != NULL) {
      js_builder_line(jsb, "%s: { type: '%s'; data: %s };", event->name, event->name, event->type);
    } else {
      js_builder_line(jsb, "%s: { type: '%s' };", event->name, event->name);
    }
  }
  // The events the states handle without a declaration, which the
  // compiler warns about, can carry anything.
  for(StringListIterator it = string_list_iterator(&inventory->events); string_list_next(&it);) {
    if(printer_find_event(inventory->machine, it.node->value) == NULL) {
      js_builder_line(jsb, "%s: { type: '%s'; data?: unknown };", it.node->value, it.node->value);
    }
  }
  js_builder_close(jsb, "}");
  js_builder_line(jsb, "export type %sEventName = keyof %sEvents;", prefix, prefix);
  js_builder_line(jsb, "export type %sEvent = %sEvents[%sEventName];", prefix, prefix, prefix);
  if(inventory->uses_any_event) {
    js_builder_add_format(jsb, "export type %sAnyEvent = %sEvent | { type: 'start' }", prefix, prefix);
    if(inventory->has_delay) {
      js_builder_add_str(jsb, " | { type: 'delay' }");
    }
    if(inventory->has_done) {
      js_builder_add_str(jsb, " | { type: 'done'; data?: unknown }");
    }
    if(inventory->has_error) {
      js_builder_add_str(jsb, " | { type: 'error'; data: unknown }");
    }
    js_builder_add_str(jsb, ";\n");
  }
}

// The names send() takes on their own, of a machine that declares its
// events: the ones that need no data.
static void add_bare_event_names(Inventory* inventory, JSBuilder* jsb) {
  for(JSEvent* event = inventory->machine->events; event != NULL; event = event->next) {
    if(event->type == NULL) {
      js_builder_add_format(jsb, " | '%s'", event->name);
    }
  }
  for(StringListIterator it = string_list_iterator(&inventory->events); string_list_next(&it);) {
    if(printer_find_event(inventory->machine, it.node->value) == NULL) {
      js_builder_add_format(jsb, " | '%s'", it.node->value);
    }
  }
}

static void print_machine(Printer* printer, JSBuilder* jsb, JSMachine* machine) {
  // Light for `machine light`, Machine for the unnamed machine: LightState,
  // CreateLightOptions, LightMachine; MachineState, CreateMachineOptions,
  // Machine.
  const char* prefix = machine->name != NULL ? printer_capitalize(printer, machine->name) : "Machine";
  Inventory inventory = { .printer = printer, .machine = machine, .prefix = prefix };
  Arena* arena = printer->arena;
  bag_init(&inventory.actions, arena);
  bag_init(&inventory.assigns, arena);
  bag_init(&inventory.guards, arena);
  bag_init(&inventory.delays, arena);
  bag_init(&inventory.services, arena);
  bag_init(&inventory.actors, arena);
  string_list_init(&inventory.events, arena);
  str_set_init(&inventory.event_set, arena);
  string_list_init(&inventory.keys, arena);
  str_set_init(&inventory.key_set, arena);
  collect_states(&inventory, machine->states);
  inventory.uses_any_event = bag_uses_any_event(&inventory.actions) || bag_uses_any_event(&inventory.assigns) ||
                             bag_uses_any_event(&inventory.guards) || bag_uses_any_event(&inventory.delays) ||
                             inventory.services.head != NULL;

  const char* machine_type = machine->name != NULL ? printer_concat(printer, prefix, "Machine") : "Machine";
  const char* options_type = printer_concat(printer, printer_concat(printer, "Create", prefix), "Options");
  bool typed = is_typed(&inventory);
  bool has_events = inventory.events.length > 0;
  bool has_keys = inventory.keys.length > 0;
  const char* event_constraint = has_events ? printer_concat(printer, printer_concat(printer, "{ type: ", prefix), "EventName }") : "{ type: string }";
  const char* context_constraint = has_keys ? printer_concat(printer, printer_concat(printer, "Record<", prefix), "ContextKey, any>") : "Record<string, any>";

  // The type parameters as they are declared, and as they are passed on.
  // A machine that declares its events has no TEvent.
  StrBuilder* declared = str_builder_create();
  str_builder_add_str(declared, "<TContext extends ");
  str_builder_add_str(declared, context_constraint);
  if(!typed) {
    str_builder_add_str(declared, ", TEvent extends ");
    str_builder_add_str(declared, event_constraint);
  }
  str_builder_add_str(declared, ">");
  const char* parameters = arena_strdup(arena, str_builder_peek(declared));
  str_builder_destroy(declared);
  const char* arguments = typed ? "<TContext>" : "<TContext, TEvent>";

  if(machine->states != NULL) {
    js_builder_add_format(jsb, "export type %sState = ", prefix);
    bool first = true;
    add_state_names(jsb, machine->states, &first);
    js_builder_add_str(jsb, ";\n");
  }
  if(typed) {
    print_event_types(&inventory, jsb);
  } else if(has_events) {
    js_builder_add_format(jsb, "export type %sEventName = ", prefix);
    for(StringListIterator it = string_list_iterator(&inventory.events); string_list_next(&it);) {
      js_builder_add_format(jsb, "%s'%s'", it.index > 0 ? " | " : "", it.node->value);
    }
    js_builder_add_str(jsb, ";\n");
  }
  if(has_keys) {
    js_builder_add_format(jsb, "export type %sContextKey = ", prefix);
    for(StringListIterator it = string_list_iterator(&inventory.keys); string_list_next(&it);) {
      js_builder_add_format(jsb, "%s'%s'", it.index > 0 ? " | " : "", it.node->value);
    }
    js_builder_add_str(jsb, ";\n");
  }
  // What assign(key) stores from the data of declared events.
  if(inventory.event_data != NULL) {
    js_builder_open(jsb, "export interface %sEventData {", prefix);
    for(EventData* data = inventory.event_data; data != NULL; data = data->next) {
      js_builder_add_indent(jsb);
      js_builder_add_format(jsb, "%s: ", data->key);
      for(StringListIterator it = string_list_iterator(&data->types); string_list_next(&it);) {
        js_builder_add_format(jsb, "%s%s", it.index > 0 ? " | " : "", it.node->value);
      }
      js_builder_add_str(jsb, ";\n");
    }
    js_builder_close(jsb, "}");
  }
  js_builder_blank(jsb);

  js_builder_open(jsb, "export interface %s%s {", options_type, parameters);
  if(inventory.event_data != NULL) {
    // The context's own type, except that a key set from an event's data
    // has to be able to hold it. Where it can't, the key's type here is
    // the data's, which the caller's value doesn't fit, or never when the
    // value would fit because the key's type is only part of the data's.
    js_builder_line(jsb, "context?: { [K in keyof TContext]: K extends keyof %sEventData ? "
                         "(%sEventData[K] extends TContext[K] ? TContext[K] : "
                         "TContext[K] extends %sEventData[K] ? never : %sEventData[K]) : TContext[K] };",
      prefix, prefix, prefix, prefix);
  } else {
    js_builder_line(jsb, "context?: TContext;");
  }
  print_bag(&inventory, jsb, &inventory.actions, "actions", "void");
  print_bag(&inventory, jsb, &inventory.assigns, "assigns", "unknown");
  print_bag(&inventory, jsb, &inventory.delays, "delays", "number");
  print_bag(&inventory, jsb, &inventory.guards, "guards", "boolean");
  if(inventory.services.head != NULL || inventory.actors.head != NULL) {
    js_builder_open(jsb, "services: {");
    for(Symbol* symbol = inventory.services.head; symbol != NULL; symbol = symbol->next) {
      if(typed) {
        js_builder_line(jsb, "%s: (context: TContext, event: %sAnyEvent) => Promise<unknown>;", symbol->name, prefix);
      } else {
        js_builder_line(jsb, "%s: (context: TContext, event: TEvent) => Promise<unknown>;", symbol->name);
      }
    }
    for(Symbol* symbol = inventory.actors.head; symbol != NULL; symbol = symbol->next) {
      if(str_map_get(inventory.services.by_name, symbol->name) == NULL) {
        js_builder_line(jsb, "%s: () => { start(): unknown; stop(): void; send(event: unknown): void };", symbol->name);
      }
    }
    js_builder_close(jsb, "};");
  }
  js_builder_close(jsb, "}");
  js_builder_blank(jsb);

  js_builder_open(jsb, "export interface %s%s {", machine_type, parameters);
  if(machine->states != NULL) {
    js_builder_line(jsb, "state: %sState;", prefix);
  }
  js_builder_line(jsb, "context: TContext;");
  if(machine->has_final) {
    js_builder_line(jsb, "done: boolean;");
  }
  if(typed) {
    js_builder_add_indent(jsb);
    js_builder_add_format(jsb, "send(event: %sEvent", prefix);
    add_bare_event_names(&inventory, jsb);
    js_builder_add_str(jsb, "): void;\n");
  } else {
    js_builder_line(jsb, "send(event: TEvent | TEvent['type']): void;");
  }
  js_builder_line(jsb, "subscribe(listener: (machine: %s%s) => void): () => void;", machine_type, arguments);
  js_builder_line(jsb, "start(): %s%s;", machine_type, arguments);
  js_builder_line(jsb, "stop(): void;");
  js_builder_close(jsb, "}");
  js_builder_blank(jsb);

  // The options are needed when the machine names something the caller
  // has to provide.
  bool needs_options = inventory.actions.head != NULL || inventory.assigns.head != NULL ||
                       inventory.guards.head != NULL || inventory.delays.head != NULL ||
                       inventory.services.head != NULL || inventory.actors.head != NULL;
  js_builder_add_format(jsb, "export %s %s<TContext extends %s = %s",
    machine->name != NULL ? "function" : "default function", machine->factory,
    context_constraint, context_constraint);
  if(!typed) {
    js_builder_add_format(jsb, ", TEvent extends %s = %s", event_constraint, event_constraint);
  }
  js_builder_add_format(jsb, ">(options%s: %s%s): %s%s;\n",
    needs_options ? "" : "?", options_type, arguments, machine_type, arguments);
}

// The `use` lines that name types of events, as type imports.
static bool print_type_imports(Printer* printer, JSBuilder* jsb) {
  bool printed = false;
  for(Node* node = printer->program->body; node != NULL; node = node->next) {
    if(node->type != NODE_IMPORT) {
      continue;
    }
    bool any = false;
    for(Node* child = node->child; child != NULL; child = child->next) {
      const char* name = ((ImportSpecifierNode*)child)->imported;
      if(printer_is_type_name(printer, name)) {
        js_builder_add_str(jsb, any ? ", " : "import type { ");
        js_builder_add_str(jsb, name);
        any = true;
      }
    }
    if(any) {
      // The source keeps its quotes.
      js_builder_add_format(jsb, " } from %s;\n", ((ImportNode*)node)->from);
      printed = true;
    }
  }
  return printed;
}

char* printer_print_dts(Printer* printer) {
  JSBuilder* jsb = js_builder_create();
  bool imports = print_type_imports(printer, jsb);
  for(JSMachine* machine = printer->machines; machine != NULL; machine = machine->next) {
    if(imports || machine != printer->machines) {
      js_builder_blank(jsb);
    }
    print_machine(printer, jsb, machine);
  }
  return js_builder_release(jsb);
}
