#include "printer.h"

#include <stdbool.h>
#include <string.h>

#include "../support/str_set.h"

// The options the factory reads: the ones its interface has symbols in.
static void add_factory_params(Printer* printer, const JSInterface* interface) {
  JSBuilder* jsb = printer->jsb;
  js_builder_add_str(jsb, "{ ");
  if(interface->actions.head != NULL) {
    js_builder_add_str(jsb, "actions, ");
  }
  if(interface->assigns.head != NULL) {
    js_builder_add_str(jsb, "assigns, ");
  }
  js_builder_add_str(jsb, "context = {}");
  if(interface->delays.head != NULL) {
    js_builder_add_str(jsb, ", delays");
  }
  if(interface->guards.head != NULL) {
    js_builder_add_str(jsb, ", guards");
  }
  if(interface->takes_parent) {
    js_builder_add_str(jsb, ", parent");
    if(strcmp(printer->parent, "parent") != 0) {
      js_builder_add_format(jsb, ": %s", printer->parent);
    }
  }
  if(interface->services.head != NULL || interface->actors.head != NULL) {
    js_builder_add_str(jsb, ", services");
  }
  js_builder_add_format(jsb, " }%s = {}", printer_any(printer));
}

// The variables for the timers, promises and children of the states.
static void print_resource_declarations(Printer* printer, JSState* state) {
  JSBuilder* jsb = printer->jsb;
  for(; state != NULL; state = state->next) {
    for(size_t i = 0; i < state->delay_count; i++) {
      js_builder_line(jsb, "let %s%s;", printer_timer_name(printer, state, i),
        printer->ts ? ": ReturnType<typeof setTimeout> | undefined" : "");
    }
    if(state->invoke_kind == INVOKE_PROMISE) {
      js_builder_line(jsb, "let %sPromise%s;", state->var, printer_any(printer));
    } else if(state->invoke_kind == INVOKE_MACHINE) {
      js_builder_line(jsb, "let %sChild%s;", state->var, printer_any(printer));
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
    if(assign == NULL || !str_set_add(keys, assign->key)) {
      continue;
    }
    // stop() stops the actors the machine spawned into its context: the
    // one a key holds, or each in the list an append made.
    if(assign->append) {
      js_builder_open(printer->jsb, "for(const %s of machine.context.%s ?? []) {", printer->actor, assign->key);
      js_builder_line(printer->jsb, "%s.stop();", printer->actor);
      js_builder_close(printer->jsb, "}");
    } else {
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

// run(step): a transition that send() didn't start, for a machine that
// queues its events. The events sent during it are handled after it.
static void print_run(Printer* printer, JSMachine* machine) {
  JSBuilder* jsb = printer->jsb;
  js_builder_open(jsb, "function %s(step%s) {", printer->run, printer->ts ? ": () => void" : "");
  if(machine->run_nests) {
    // Already in a transition, which step is then part of.
    js_builder_open(jsb, "if(%s) {", printer->busy);
    js_builder_line(jsb, "step();");
    js_builder_line(jsb, "return;");
    js_builder_close(jsb, "}");
  }
  js_builder_line(jsb, "%s = true;", printer->busy);
  js_builder_open(jsb, "try {");
  js_builder_line(jsb, "step();");
  js_builder_line(jsb, "notify();");
  js_builder_line(jsb, "%s = false;", printer->busy);
  js_builder_open(jsb, "if(%s.length) {", printer->queue);
  js_builder_line(jsb, "machine.send(%s.shift());", printer->queue);
  js_builder_close(jsb, "}");
  js_builder_close(jsb, "} finally {");
  jsb->depth++;
  js_builder_line(jsb, "%s.length = 0;", printer->queue);
  js_builder_line(jsb, "%s = false;", printer->busy);
  js_builder_close(jsb, "}");
  js_builder_close(jsb, "}");
}

void printer_print_machine(Printer* printer, JSMachine* machine) {
  JSBuilder* jsb = printer->jsb;
  printer->machine = machine;

  if(printer->ts) {
    // The types end with the factory's declaration, which the function
    // below then implements.
    printer_print_machine_types(printer, jsb, machine);
  }

  js_builder_add_str(jsb, machine->name == NULL ? "export default function " : "export function ");
  js_builder_add_format(jsb, "%s(", machine->factory);
  add_factory_params(printer, machine->interface);
  js_builder_add_format(jsb, ")%s {\n", printer_any(printer));
  jsb->depth++;

  printer_print_view_setup(printer, machine);
  js_builder_line(jsb, "const listeners = new Set%s();", printer->ts ? "<(machine: any) => void>" : "");
  if(machine->queued) {
    js_builder_line(jsb, "let %s = false;", printer->busy);
    js_builder_line(jsb, "const %s%s = [];", printer->queue, printer->ts ? ": any[]" : "");
  }
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

  if(machine->needs_run) {
    print_run(printer, machine);
  }

  js_builder_open(jsb, "const machine%s = {", printer_any(printer));
  if(machine->states != NULL) {
    js_builder_line(jsb, "state: '%s',", printer_leaf(machine->initial)->path);
  }
  js_builder_line(jsb, "context,");
  if(machine->view != NULL && printer->ssr) {
    // The view as HTML, for the server to send.
    js_builder_line(jsb, strcmp(printer->render, "render") == 0 ? "render," : "render: %s,", printer->render);
  } else if(machine->view != NULL) {
    // The view, for the host to attach, and its top-level nodes, which
    // stay when attaching empties the fragment.
    js_builder_line(jsb, strcmp(printer->dom, "dom") == 0 ? "dom," : "dom: %s,", printer->dom);
    js_builder_line(jsb, strcmp(printer->nodes, "nodes") == 0 ? "nodes," : "nodes: %s,", printer->nodes);
  }
  if(machine->has_final) {
    js_builder_line(jsb, "done: false,");
  }
  if(machine->states != NULL) {
    js_builder_open(jsb, "send(%s) {", printer_event_param(printer, true));
    js_builder_open(jsb, "if(typeof event === 'string') {");
    js_builder_line(jsb, "event = { type: event };");
    js_builder_close(jsb, "}");
    if(machine->queued) {
      js_builder_open(jsb, "if(%s) {", printer->busy);
      js_builder_line(jsb, "%s.push(event);", printer->queue);
      js_builder_line(jsb, "return;");
      js_builder_close(jsb, "}");
      js_builder_line(jsb, "%s = true;", printer->busy);
      js_builder_open(jsb, "try {");
      js_builder_open(jsb, "do {");
    }
    js_builder_line(jsb, "const handler = states[machine.state][event.type];");
    js_builder_open(jsb, "if(handler) {");
    js_builder_line(jsb, "handler(event);");
    js_builder_line(jsb, "notify();");
    js_builder_close(jsb, "}");
    if(machine->queued) {
      js_builder_close(jsb, "} while((event = %s.shift()));", printer->queue);
      js_builder_close(jsb, "} finally {");
      jsb->depth++;
      js_builder_line(jsb, "%s.length = 0;", printer->queue);
      js_builder_line(jsb, "%s = false;", printer->busy);
      js_builder_close(jsb, "}");
    }
    js_builder_close(jsb, "},");
  } else {
    js_builder_line(jsb, "send() {},");
  }
  js_builder_open(jsb, "subscribe(listener%s) {", printer_any(printer));
  js_builder_line(jsb, "listeners.add(listener);");
  js_builder_line(jsb, "return () => listeners.delete(listener);");
  js_builder_close(jsb, "},");
  js_builder_open(jsb, "start() {");
  if(machine->initial != NULL && (machine->initial->flags & STATE_NEEDS_ENTER)) {
    bool has_event = machine->initial->flags & STATE_ENTER_TAKES_EVENT;
    if(!machine->queued) {
      js_builder_line(jsb, "enter%s(%s);", machine->initial->ident, has_event ? "{ type: 'start' }" : "");
      js_builder_line(jsb, "notify();");
    } else if(has_event) {
      js_builder_line(jsb, "%s(() => enter%s({ type: 'start' }));", printer->run, machine->initial->ident);
    } else {
      js_builder_line(jsb, "%s(enter%s);", printer->run, machine->initial->ident);
    }
  } else {
    js_builder_line(jsb, "notify();");
  }
  js_builder_line(jsb, "return machine;");
  js_builder_close(jsb, "},");
  js_builder_open(jsb, "stop() {");
  print_resource_cleanup(printer, machine->states);
  StrSet keys;
  str_set_init(&keys, printer->arena);
  collect_spawn_keys(printer, machine->states, &keys);
  if(machine->queued) {
    js_builder_line(jsb, "%s.length = 0;", printer->queue);
  }
  js_builder_line(jsb, "listeners.clear();");
  js_builder_close(jsb, "}");
  js_builder_close(jsb, "};");
  js_builder_line(jsb, "return machine;");
  js_builder_close(jsb, "}");
}
