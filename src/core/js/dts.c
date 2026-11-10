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

typedef struct Inventory {
  Printer* printer;
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
} Inventory;

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

static void collect_actions(Inventory* inventory, TransitionAction* action, const char* event) {
  for(; action != NULL; action = action->next) {
    Expression* expression = printer_action_expression(inventory->printer, action);
    if(expression == NULL) {
      continue;
    }
    if(expression->type == EXPRESSION_ASSIGN) {
      AssignExpression* assign = (AssignExpression*)expression;
      add_key(inventory, assign->key);
      if(assign->value != NULL && assign->value->type == EXPRESSION_SYMBOL) {
        bag_add(inventory, &inventory->assigns, ((SymbolExpression*)assign->value)->name, assign->key, event);
      } else if(assign->value != NULL && assign->value->type == EXPRESSION_SPAWN) {
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
    for(TransitionNode* t = node->delay_transition; t != NULL; t = t->next) {
      DelayExpression* delay = t->delay != NULL ? t->delay->expression : NULL;
      if(delay != NULL && delay->ref != NULL && delay->ref->type == EXPRESSION_SYMBOL) {
        bag_add(inventory, &inventory->delays, ((SymbolExpression*)delay->ref)->name, NULL, NULL);
      }
    }
    collect_transitions(inventory, node->delay_transition, false);
    if(node->invoke != NULL) {
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
static void add_event_type(JSBuilder* jsb, Symbol* symbol) {
  if(symbol->any_event || symbol->events.length == 0) {
    js_builder_add_str(jsb, "TEvent");
    return;
  }
  js_builder_add_str(jsb, "Extract<TEvent, { type: ");
  for(StringListIterator it = string_list_iterator(&symbol->events); string_list_next(&it);) {
    js_builder_add_format(jsb, "%s'%s'", it.index > 0 ? " | " : "", it.node->value);
  }
  js_builder_add_str(jsb, " }>");
}

// A bag of the options interface: `actions: { log: (...) => void; };`.
static void print_bag(JSBuilder* jsb, Bag* bag, const char* name, const char* returns, bool takes_event) {
  if(bag->head == NULL) {
    return;
  }
  js_builder_open(jsb, "%s: {", name);
  for(Symbol* symbol = bag->head; symbol != NULL; symbol = symbol->next) {
    js_builder_add_indent(jsb);
    js_builder_add_format(jsb, "%s: (", symbol->name);
    if(takes_event) {
      js_builder_add_str(jsb, "context: TContext, event: ");
      add_event_type(jsb, symbol);
    }
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

static void print_machine(Printer* printer, JSBuilder* jsb, JSMachine* machine) {
  Inventory inventory = { .printer = printer };
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

  // Light for `machine light`, Machine for the unnamed machine: LightState,
  // CreateLightOptions, LightMachine; MachineState, CreateMachineOptions,
  // Machine.
  const char* prefix = machine->name != NULL ? printer_capitalize(printer, machine->name) : "Machine";
  const char* machine_type = machine->name != NULL ? printer_concat(printer, prefix, "Machine") : "Machine";
  const char* options_type = printer_concat(printer, printer_concat(printer, "Create", prefix), "Options");
  bool has_events = inventory.events.length > 0;
  bool has_keys = inventory.keys.length > 0;
  const char* event_constraint = has_events ? printer_concat(printer, printer_concat(printer, "{ type: ", prefix), "EventName }") : "{ type: string }";
  const char* context_constraint = has_keys ? printer_concat(printer, printer_concat(printer, "Record<", prefix), "ContextKey, any>") : "Record<string, any>";

  if(machine->states != NULL) {
    js_builder_add_format(jsb, "export type %sState = ", prefix);
    bool first = true;
    add_state_names(jsb, machine->states, &first);
    js_builder_add_str(jsb, ";\n");
  }
  if(has_events) {
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
  js_builder_blank(jsb);

  js_builder_open(jsb, "export interface %s<TContext extends %s, TEvent extends %s> {", options_type, context_constraint, event_constraint);
  js_builder_line(jsb, "context?: TContext;");
  print_bag(jsb, &inventory.actions, "actions", "void", true);
  print_bag(jsb, &inventory.assigns, "assigns", "unknown", true);
  print_bag(jsb, &inventory.delays, "delays", "number", true);
  print_bag(jsb, &inventory.guards, "guards", "boolean", true);
  if(inventory.services.head != NULL || inventory.actors.head != NULL) {
    js_builder_open(jsb, "services: {");
    for(Symbol* symbol = inventory.services.head; symbol != NULL; symbol = symbol->next) {
      js_builder_line(jsb, "%s: (context: TContext, event: TEvent) => Promise<unknown>;", symbol->name);
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

  js_builder_open(jsb, "export interface %s<TContext extends %s, TEvent extends %s> {", machine_type, context_constraint, event_constraint);
  if(machine->states != NULL) {
    js_builder_line(jsb, "state: %sState;", prefix);
  }
  js_builder_line(jsb, "context: TContext;");
  if(machine->has_final) {
    js_builder_line(jsb, "done: boolean;");
  }
  js_builder_line(jsb, "send(event: TEvent | TEvent['type']): void;");
  js_builder_line(jsb, "subscribe(listener: (machine: %s<TContext, TEvent>) => void): () => void;", machine_type);
  js_builder_line(jsb, "start(): %s<TContext, TEvent>;", machine_type);
  js_builder_line(jsb, "stop(): void;");
  js_builder_close(jsb, "}");
  js_builder_blank(jsb);

  // The options are needed when the machine names something the caller
  // has to provide.
  bool needs_options = inventory.actions.head != NULL || inventory.assigns.head != NULL ||
                       inventory.guards.head != NULL || inventory.delays.head != NULL ||
                       inventory.services.head != NULL || inventory.actors.head != NULL;
  js_builder_add_format(jsb, "export %s %s<TContext extends %s = %s, TEvent extends %s = %s>(options%s: %s<TContext, TEvent>): %s<TContext, TEvent>;\n",
    machine->name != NULL ? "function" : "default function", machine->factory,
    context_constraint, context_constraint, event_constraint, event_constraint,
    needs_options ? "" : "?", options_type, machine_type);
}

char* printer_print_dts(Printer* printer) {
  JSBuilder* jsb = js_builder_create();
  for(JSMachine* machine = printer->machines; machine != NULL; machine = machine->next) {
    if(machine != printer->machines) {
      js_builder_blank(jsb);
    }
    print_machine(printer, jsb, machine);
  }
  return js_builder_release(jsb);
}
