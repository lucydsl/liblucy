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

typedef struct Inventory {
  Printer* printer;
  JSMachine* machine;
  // What the machine needs and handles (interface.c).
  JSInterface* interface;
  // The prefix of the machine's types: Light, or Machine when unnamed.
  const char* prefix;
  // Whether anything is typed with every event, the machine's own included.
  bool uses_any_event;
} Inventory;

// Whether the machine declares its events.
static bool is_typed(const Inventory* inventory) {
  return inventory->machine->events != NULL;
}

// The event type a symbol sees: the events it is used on, or any.
static void add_event_type(Inventory* inventory, JSBuilder* jsb, JSSymbol* symbol) {
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
static void print_bag(Inventory* inventory, JSBuilder* jsb, JSSymbolBag* bag, const char* name, const char* returns) {
  if(bag->head == NULL) {
    return;
  }
  js_builder_open(jsb, "%s: {", name);
  for(JSSymbol* symbol = bag->head; symbol != NULL; symbol = symbol->next) {
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
static bool bag_uses_any_event(JSSymbolBag* bag) {
  for(JSSymbol* symbol = bag->head; symbol != NULL; symbol = symbol->next) {
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
  for(StringListIterator it = string_list_iterator(&inventory->interface->events); string_list_next(&it);) {
    if(printer_find_event(inventory->machine, it.node->value) == NULL) {
      js_builder_line(jsb, "%s: { type: '%s'; data?: unknown };", it.node->value, it.node->value);
    }
  }
  js_builder_close(jsb, "}");
  js_builder_line(jsb, "export type %sEventName = keyof %sEvents;", prefix, prefix);
  js_builder_line(jsb, "export type %sEvent = %sEvents[%sEventName];", prefix, prefix, prefix);
  if(inventory->uses_any_event) {
    js_builder_add_format(jsb, "export type %sAnyEvent = %sEvent | { type: 'start' }", prefix, prefix);
    if(inventory->interface->has_delay) {
      js_builder_add_str(jsb, " | { type: 'delay' }");
    }
    if(inventory->interface->has_done) {
      js_builder_add_str(jsb, " | { type: 'done'; data?: unknown }");
    }
    if(inventory->interface->has_error) {
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
  for(StringListIterator it = string_list_iterator(&inventory->interface->events); string_list_next(&it);) {
    if(printer_find_event(inventory->machine, it.node->value) == NULL) {
      js_builder_add_format(jsb, " | '%s'", it.node->value);
    }
  }
}

void printer_print_machine_types(Printer* printer, JSBuilder* jsb, JSMachine* machine) {
  // Light for `machine light`, Machine for the unnamed machine: LightState,
  // CreateLightOptions, LightMachine; MachineState, CreateMachineOptions,
  // Machine.
  const char* prefix = machine->name != NULL ? printer_capitalize(printer, machine->name) : "Machine";
  JSInterface* interface = machine->interface;
  Inventory inventory = { .printer = printer, .machine = machine, .interface = interface, .prefix = prefix };
  Arena* arena = printer->arena;
  inventory.uses_any_event = bag_uses_any_event(&interface->actions) || bag_uses_any_event(&interface->assigns) ||
                             bag_uses_any_event(&interface->guards) || bag_uses_any_event(&interface->delays) ||
                             interface->services.head != NULL;

  const char* machine_type = machine->name != NULL ? printer_concat(printer, prefix, "Machine") : "Machine";
  const char* options_type = printer_concat(printer, printer_concat(printer, "Create", prefix), "Options");
  bool typed = is_typed(&inventory);
  bool has_events = interface->events.length > 0;
  bool has_keys = interface->keys.length > 0;
  const char* event_constraint = has_events ? printer_concat(printer, printer_concat(printer, "{ type: ", prefix), "EventName }") : "{ type: string }";
  const char* context_constraint = has_keys ? printer_concat(printer, printer_concat(printer, "Record<", prefix), "ContextKey, any>") : "Record<string, any>";
  // A machine with inputs has to be given a context that holds them.
  bool has_inputs = machine->inputs != NULL;
  if(has_inputs) {
    context_constraint = printer_concat(printer, printer_concat(printer, context_constraint, " & "), printer_concat(printer, prefix, "Input"));
  }

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
    for(StringListIterator it = string_list_iterator(&interface->events); string_list_next(&it);) {
      js_builder_add_format(jsb, "%s'%s'", it.index > 0 ? " | " : "", it.node->value);
    }
    js_builder_add_str(jsb, ";\n");
  }
  if(has_keys) {
    js_builder_add_format(jsb, "export type %sContextKey = ", prefix);
    for(StringListIterator it = string_list_iterator(&interface->keys); string_list_next(&it);) {
      js_builder_add_format(jsb, "%s'%s'", it.index > 0 ? " | " : "", it.node->value);
    }
    js_builder_add_str(jsb, ";\n");
  }
  if(has_inputs) {
    js_builder_open(jsb, "export interface %sInput {", prefix);
    for(JSInput* input = machine->inputs; input != NULL; input = input->next) {
      js_builder_line(jsb, "%s: %s;", input->name, input->type != NULL ? input->type : "unknown");
    }
    js_builder_close(jsb, "}");
  }
  // What assign(key) stores from the data of declared events.
  if(interface->event_data != NULL) {
    js_builder_open(jsb, "export interface %sEventData {", prefix);
    for(JSEventData* data = interface->event_data; data != NULL; data = data->next) {
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
  if(interface->event_data != NULL) {
    // The context's own type, except that a key set from an event's data
    // has to be able to hold it. Where it can't, the key's type here is
    // the data's, which the caller's value doesn't fit, or never when the
    // value would fit because the key's type is only part of the data's.
    js_builder_line(jsb, "context%s: { [K in keyof TContext]: K extends keyof %sEventData ? "
                         "(%sEventData[K] extends TContext[K] ? TContext[K] : "
                         "TContext[K] extends %sEventData[K] ? never : %sEventData[K]) : TContext[K] };",
      has_inputs ? "" : "?", prefix, prefix, prefix, prefix);
  } else {
    js_builder_line(jsb, "context%s: TContext;", has_inputs ? "" : "?");
  }
  print_bag(&inventory, jsb, &interface->actions, "actions", "void");
  print_bag(&inventory, jsb, &interface->assigns, "assigns", "unknown");
  print_bag(&inventory, jsb, &interface->delays, "delays", "number");
  print_bag(&inventory, jsb, &interface->guards, "guards", "boolean");
  if(interface->parent_events.length > 0) {
    // Whatever created the machine, which it sends these events to.
    js_builder_add_indent(jsb);
    js_builder_add_str(jsb, "parent?: { send(event: { type: ");
    for(StringListIterator it = string_list_iterator(&interface->parent_events); string_list_next(&it);) {
      js_builder_add_format(jsb, "%s'%s'", it.index > 0 ? " | " : "", it.node->value);
    }
    js_builder_add_str(jsb, "; data?: unknown }): void };\n");
  } else if(interface->takes_parent) {
    // It only hands its parent on, to a machine it spawns.
    js_builder_line(jsb, "parent?: unknown;");
  }
  if(interface->services.head != NULL || interface->actors.head != NULL) {
    js_builder_open(jsb, "services: {");
    for(JSSymbol* symbol = interface->services.head; symbol != NULL; symbol = symbol->next) {
      if(typed) {
        js_builder_line(jsb, "%s: (context: TContext, event: %sAnyEvent) => Promise<unknown>;", symbol->name, prefix);
      } else {
        js_builder_line(jsb, "%s: (context: TContext, event: TEvent) => Promise<unknown>;", symbol->name);
      }
    }
    for(JSSymbol* symbol = interface->actors.head; symbol != NULL; symbol = symbol->next) {
      if(str_map_get(interface->services.by_name, symbol->name) == NULL) {
        js_builder_line(jsb, "%s: (options: { parent: { send(event: unknown): void } }) => "
                             "{ start(): unknown; stop(): void; send(event: unknown): void };",
          symbol->name);
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

  bool needs_options = printer_interface_needs_options(interface) || has_inputs;
  js_builder_add_format(jsb, "export %s %s<TContext extends %s = %s",
    machine->name != NULL ? "function" : "default function", machine->factory,
    context_constraint, context_constraint);
  if(!typed) {
    js_builder_add_format(jsb, ", TEvent extends %s = %s", event_constraint, event_constraint);
  }
  js_builder_add_format(jsb, ">(options%s: %s%s): %s%s;\n",
    needs_options ? "" : "?", options_type, arguments, machine_type, arguments);
}

bool printer_print_type_imports(Printer* printer, JSBuilder* jsb) {
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
  bool imports = printer_print_type_imports(printer, jsb);
  for(JSMachine* machine = printer->machines; machine != NULL; machine = machine->next) {
    if(imports || machine != printer->machines) {
      js_builder_blank(jsb);
    }
    printer_print_machine_types(printer, jsb, machine);
  }
  return js_builder_release(jsb);
}
