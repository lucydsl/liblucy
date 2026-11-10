#include "printer.h"

#include <stdbool.h>

#include "../support/str_set.h"

static void add_factory_params(JSBuilder* jsb, MachineNode* machine_node) {
  js_builder_add_str(jsb, "{ ");
  if(machine_node->flags & MACHINE_USES_ACTION) {
    js_builder_add_str(jsb, "actions, ");
  }
  if(machine_node->flags & MACHINE_USES_ASSIGN) {
    js_builder_add_str(jsb, "assigns, ");
  }
  js_builder_add_str(jsb, "context = {}");
  if(machine_node->flags & MACHINE_USES_DELAY) {
    js_builder_add_str(jsb, ", delays");
  }
  if(machine_node->flags & MACHINE_USES_GUARD) {
    js_builder_add_str(jsb, ", guards");
  }
  if(machine_node->flags & MACHINE_USES_SERVICE) {
    js_builder_add_str(jsb, ", services");
  }
  js_builder_add_str(jsb, " } = {}");
}

// The variables for the timers, promises and children of the states.
static void print_resource_declarations(Printer* printer, JSState* state) {
  JSBuilder* jsb = printer->jsb;
  for(; state != NULL; state = state->next) {
    for(size_t i = 0; i < state->delay_count; i++) {
      js_builder_line(jsb, "let %s;", printer_timer_name(printer, state, i));
    }
    if(state->invoke_kind == INVOKE_PROMISE) {
      js_builder_line(jsb, "let %sPromise;", state->var);
    } else if(state->invoke_kind == INVOKE_MACHINE) {
      js_builder_line(jsb, "let %sChild;", state->var);
    }
    print_resource_declarations(printer, state->child);
  }
}

// What stop() does about them.
static void print_resource_cleanup(Printer* printer, JSState* state) {
  JSBuilder* jsb = printer->jsb;
  for(; state != NULL; state = state->next) {
    for(size_t i = 0; i < state->delay_count; i++) {
      js_builder_line(jsb, "clearTimeout(%s);", printer_timer_name(printer, state, i));
    }
    if(state->invoke_kind == INVOKE_PROMISE) {
      js_builder_line(jsb, "%sPromise = undefined;", state->var);
    } else if(state->invoke_kind == INVOKE_MACHINE) {
      js_builder_open(jsb, "if(%sChild) {", state->var);
      js_builder_line(jsb, "%sChild.stop();", state->var);
      js_builder_close(jsb, "}");
    }
    print_resource_cleanup(printer, state->child);
  }
}

static void collect_spawn_keys_in(Printer* printer, TransitionAction* action, StrSet* keys) {
  for(; action != NULL; action = action->next) {
    AssignExpression* assign = printer_action_spawn(printer, action);
    if(assign != NULL && str_set_add(keys, assign->key)) {
      // stop() stops the actors the machine spawned into its context.
      js_builder_open(printer->jsb, "if(machine.context.%s) {", assign->key);
      js_builder_line(printer->jsb, "machine.context.%s.stop();", assign->key);
      js_builder_close(printer->jsb, "}");
    }
  }
}

static void collect_spawn_keys(Printer* printer, JSState* state, StrSet* keys) {
  for(; state != NULL; state = state->next) {
    StateNode* node = state->node;
    if(node->entry != NULL) {
      collect_spawn_keys_in(printer, node->entry->action, keys);
    }
    if(node->exit != NULL) {
      collect_spawn_keys_in(printer, node->exit->action, keys);
    }
    for(TransitionNode* t = node->event_transition; t != NULL; t = t->next) {
      collect_spawn_keys_in(printer, t->action, keys);
    }
    for(TransitionNode* t = node->immediate_transition; t != NULL; t = t->next) {
      collect_spawn_keys_in(printer, t->action, keys);
    }
    for(TransitionNode* t = node->delay_transition; t != NULL; t = t->next) {
      collect_spawn_keys_in(printer, t->action, keys);
    }
    if(node->invoke != NULL) {
      for(TransitionNode* t = node->invoke->event_transition; t != NULL; t = t->next) {
        collect_spawn_keys_in(printer, t->action, keys);
      }
    }
    collect_spawn_keys(printer, state->child, keys);
  }
}

void printer_print_machine(Printer* printer, JSMachine* machine) {
  JSBuilder* jsb = printer->jsb;
  printer->machine = machine;

  js_builder_add_str(jsb, machine->name == NULL ? "export default function " : "export function ");
  js_builder_add_format(jsb, "%s(", machine->factory);
  add_factory_params(jsb, machine->node);
  js_builder_add_str(jsb, ") {\n");
  jsb->depth++;

  js_builder_line(jsb, "const listeners = new Set();");
  print_resource_declarations(printer, machine->states);
  if(machine->states != NULL) {
    printer_print_states_table(printer, machine);
  }
  printer_print_state_functions(printer, machine->states);

  js_builder_open(jsb, "function notify() {");
  js_builder_open(jsb, "for(const listener of listeners) {");
  js_builder_line(jsb, "listener(machine);");
  js_builder_close(jsb, "}");
  js_builder_close(jsb, "}");

  js_builder_open(jsb, "const machine = {");
  if(machine->states != NULL) {
    js_builder_line(jsb, "state: '%s',", printer_leaf(machine->initial)->path);
  }
  js_builder_line(jsb, "context,");
  if(machine->has_final) {
    js_builder_line(jsb, "done: false,");
  }
  if(machine->states != NULL) {
    js_builder_open(jsb, "send(event) {");
    js_builder_open(jsb, "if(typeof event === 'string') {");
    js_builder_line(jsb, "event = { type: event };");
    js_builder_close(jsb, "}");
    js_builder_line(jsb, "const handler = states[machine.state][event.type];");
    js_builder_open(jsb, "if(handler) {");
    js_builder_line(jsb, "handler(event);");
    js_builder_line(jsb, "notify();");
    js_builder_close(jsb, "}");
    js_builder_close(jsb, "},");
  } else {
    js_builder_line(jsb, "send() {},");
  }
  js_builder_open(jsb, "subscribe(listener) {");
  js_builder_line(jsb, "listeners.add(listener);");
  js_builder_line(jsb, "return () => listeners.delete(listener);");
  js_builder_close(jsb, "},");
  js_builder_open(jsb, "start() {");
  if(machine->initial != NULL && (machine->initial->flags & STATE_NEEDS_ENTER)) {
    bool has_event = machine->initial->flags & STATE_ENTER_TAKES_EVENT;
    js_builder_line(jsb, "enter%s(%s);", machine->initial->ident, has_event ? "{ type: 'start' }" : "");
  }
  js_builder_line(jsb, "notify();");
  js_builder_line(jsb, "return machine;");
  js_builder_close(jsb, "},");
  js_builder_open(jsb, "stop() {");
  print_resource_cleanup(printer, machine->states);
  StrSet keys;
  str_set_init(&keys, printer->arena);
  collect_spawn_keys(printer, machine->states, &keys);
  js_builder_line(jsb, "listeners.clear();");
  js_builder_close(jsb, "}");
  js_builder_close(jsb, "};");
  js_builder_line(jsb, "return machine;");
  js_builder_close(jsb, "}");
}
