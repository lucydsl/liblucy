#include "printer.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../support/str_set.h"

// The variable holding a state's index'th timer: greenTimer, greenTimer2.
static const char* timer_name(Printer* printer, JSState* state, size_t index) {
  const char* name = printer_concat(printer, state->var, "Timer");
  if(index == 0) {
    return name;
  }
  char suffix[24];
  snprintf(suffix, sizeof(suffix), "%zu", index + 1);
  return printer_concat(printer, name, suffix);
}

const char* printer_timer_name(Printer* printer, JSState* state, size_t index) {
  return timer_name(printer, state, index);
}

static void print_timer(Printer* printer, JSState* state, TransitionNode* transition, size_t index) {
  JSBuilder* jsb = printer->jsb;
  bool has_event = printer_transition_takes_event(printer, state, transition);
  js_builder_open(jsb, "%s = setTimeout(() => {", timer_name(printer, state, index));
  if(has_event) {
    js_builder_line(jsb, "const event = { type: 'delay' };");
  }
  if(transition->guard != NULL) {
    js_builder_add_indent(jsb);
    js_builder_add_str(jsb, "if(");
    printer_add_guards(printer, transition->guard);
    js_builder_add_str(jsb, ") {\n");
    jsb->depth++;
  }
  printer_print_transition(printer, state, transition, has_event);
  js_builder_line(jsb, "notify();");
  if(transition->guard != NULL) {
    js_builder_close(jsb, "}");
  }
  jsb->depth--;
  js_builder_add_indent(jsb);
  js_builder_add_str(jsb, "}, ");
  printer_add_delay_length(printer, transition->delay->expression);
  js_builder_add_str(jsb, ");\n");
}

static TransitionNode* find_invoke_transition(InvokeNode* invoke, const char* event) {
  for(TransitionNode* t = invoke->event_transition; t != NULL; t = t->next) {
    const char* name = printer_event_name(t);
    if(name != NULL && strcmp(name, event) == 0) {
      return t;
    }
  }
  return NULL;
}

// Warn about the events of an invoke that aren't done or error.
static void check_invoke_events(Printer* printer, InvokeNode* invoke) {
  for(TransitionNode* t = invoke->event_transition; t != NULL; t = t->next) {
    const char* name = printer_event_name(t);
    if(name != NULL && strcmp(name, "done") != 0 && strcmp(name, "error") != 0) {
      printer_warnf(printer, LUCY_STREAM_STDOUT, "Regular events in invoke are not supported. Saw event [%s]\n", name);
    }
  }
}

// The callback for a promise's result: the transition on done or error.
static void print_invoke_callback(Printer* printer, JSState* state, TransitionNode* transition, const char* type, const char* data) {
  JSBuilder* jsb = printer->jsb;
  js_builder_line(jsb, "if(%sPromise !== promise) return;", state->var);
  if(printer_handler_takes_event(printer, state, transition)) {
    js_builder_line(jsb, "const event = { type: '%s', %s };", type, data);
  }
  printer_print_event_handler(printer, state, transition, true);
  js_builder_line(jsb, "notify();");
}

static void print_invoke(Printer* printer, JSState* state) {
  JSBuilder* jsb = printer->jsb;
  InvokeNode* invoke = state->node->invoke;
  check_invoke_events(printer, invoke);
  TransitionNode* done = find_invoke_transition(invoke, "done");
  TransitionNode* error = find_invoke_transition(invoke, "error");

  if(state->invoke_kind == INVOKE_MACHINE) {
    js_builder_line(jsb, "const child = %s();", state->invoked->factory);
    js_builder_line(jsb, "%sChild = child;", state->var);
    if(done != NULL && state->invoked->has_final) {
      js_builder_open(jsb, "child.subscribe(() => {");
      js_builder_open(jsb, "if(child.done) {");
      bool has_event = printer_handler_takes_event(printer, state, done);
      if(has_event) {
        js_builder_line(jsb, "const event = { type: 'done' };");
      }
      printer_print_event_handler(printer, state, done, has_event);
      js_builder_line(jsb, "notify();");
      js_builder_close(jsb, "}");
      js_builder_close(jsb, "});");
    }
    if(error != NULL) {
      printer_warnf(printer, LUCY_STREAM_STDOUT, "State [%s] invokes a machine, which never sends error.\n", state->path);
    }
    js_builder_line(jsb, "child.start();");
    return;
  }

  Expression* ref = invoke->expr->ref;
  js_builder_add_indent(jsb);
  if(ref->type == EXPRESSION_IDENTIFIER) {
    js_builder_add_format(jsb, "const promise = %s(machine.context, event);\n", ((IdentifierExpression*)ref)->name);
  } else {
    js_builder_add_format(jsb, "const promise = services.%s(machine.context, event);\n", ((SymbolExpression*)ref)->name);
  }
  js_builder_line(jsb, "%sPromise = promise;", state->var);
  if(done != NULL) {
    bool done_event = printer_handler_takes_event(printer, state, done);
    js_builder_open(jsb, "promise.then(%s => {", done_event ? "data" : "()");
    print_invoke_callback(printer, state, done, "done", "data");
    if(error != NULL) {
      bool error_event = printer_handler_takes_event(printer, state, error);
      js_builder_close(jsb, "}, %s => {", error_event ? "error" : "()");
      jsb->depth++;
      print_invoke_callback(printer, state, error, "error", "data: error");
    }
    js_builder_close(jsb, "});");
  } else if(error != NULL) {
    bool error_event = printer_handler_takes_event(printer, state, error);
    js_builder_open(jsb, "promise.catch(%s => {", error_event ? "error" : "()");
    print_invoke_callback(printer, state, error, "error", "data: error");
    js_builder_close(jsb, "});");
  }
}

static void print_enter(Printer* printer, JSState* state) {
  JSBuilder* jsb = printer->jsb;
  StateNode* node = state->node;
  bool has_event = state->flags & STATE_ENTER_TAKES_EVENT;

  js_builder_add_indent(jsb);
  js_builder_add_format(jsb, "function enter%s(", state->ident);
  if(has_event) {
    js_builder_add_str(jsb, "event");
  }
  if(state->flags & STATE_ENTER_TAKES_CHILD) {
    js_builder_add_format(jsb, ", enterChild = enter%s", state->initial->ident);
  }
  js_builder_add_str(jsb, ") {\n");
  jsb->depth++;

  if(state->child == NULL) {
    js_builder_line(jsb, "machine.state = '%s';", state->path);
  }
  if(state->top_final) {
    js_builder_line(jsb, "machine.done = true;");
  }
  if(node->entry != NULL) {
    for(TransitionAction* action = node->entry->action; action != NULL; action = action->next) {
      printer_print_action(printer, action);
    }
  }
  size_t index = 0;
  for(TransitionNode* t = node->delay_transition; t != NULL; t = t->next) {
    print_timer(printer, state, t, index++);
  }
  if(state->invoke_kind != INVOKE_NONE) {
    print_invoke(printer, state);
  }
  if(state->child != NULL) {
    if(state->flags & STATE_ENTER_TAKES_CHILD) {
      js_builder_line(jsb, "enterChild(event);");
    } else if(state->initial->flags & STATE_NEEDS_ENTER) {
      bool child_event = state->initial->flags & STATE_ENTER_TAKES_EVENT;
      js_builder_line(jsb, "enter%s(%s);", state->initial->ident, child_event ? "event" : "");
    } else {
      js_builder_line(jsb, "machine.state = '%s';", printer_leaf(state->initial)->path);
    }
  }
  if(node->immediate_transition != NULL) {
    if(state->flags & STATE_NEEDS_ALWAYS) {
      bool always_event = printer_immediate_takes_event(printer, state);
      js_builder_line(jsb, "always%s(%s);", state->ident, always_event ? "event" : "");
    } else {
      printer_print_immediate_transitions(printer, state, has_event);
    }
  }
  js_builder_close(jsb, "}");
}

static void print_exit(Printer* printer, JSState* state) {
  JSBuilder* jsb = printer->jsb;
  StateNode* node = state->node;
  bool has_event = state->flags & STATE_EXIT_TAKES_EVENT;
  js_builder_open(jsb, "function exit%s(%s) {", state->ident, has_event ? "event" : "");

  // Leave whichever child is active first.
  bool opened = false;
  for(JSState* child = state->child; child != NULL; child = child->next) {
    if(!(child->flags & STATE_NEEDS_EXIT)) {
      continue;
    }
    if(opened) {
      jsb->depth--;
    }
    if(child->child != NULL) {
      js_builder_open(jsb, "%sif(machine.state.startsWith('%s.')) {", opened ? "} else " : "", child->path);
    } else {
      js_builder_open(jsb, "%sif(machine.state === '%s') {", opened ? "} else " : "", child->path);
    }
    opened = true;
    bool child_event = has_event && (child->flags & STATE_EXIT_TAKES_EVENT);
    js_builder_line(jsb, "exit%s(%s);", child->ident, child_event ? "event" : "");
  }
  if(opened) {
    js_builder_close(jsb, "}");
  }

  if(node->exit != NULL) {
    for(TransitionAction* action = node->exit->action; action != NULL; action = action->next) {
      printer_print_action(printer, action);
    }
  }
  for(size_t i = 0; i < state->delay_count; i++) {
    js_builder_line(jsb, "clearTimeout(%s);", timer_name(printer, state, i));
  }
  if(state->invoke_kind == INVOKE_PROMISE) {
    js_builder_line(jsb, "%sPromise = undefined;", state->var);
  } else if(state->invoke_kind == INVOKE_MACHINE) {
    js_builder_line(jsb, "%sChild.stop();", state->var);
  }
  js_builder_close(jsb, "}");
}

static void print_always(Printer* printer, JSState* state) {
  JSBuilder* jsb = printer->jsb;
  bool has_event = printer_immediate_takes_event(printer, state);
  js_builder_open(jsb, "function always%s(%s) {", state->ident, has_event ? "event" : "");
  printer_print_immediate_transitions(printer, state, has_event);
  js_builder_close(jsb, "}");
}

// A compound state's handlers are functions, shared by the table entries
// of every state nested in it.
static void print_handlers(Printer* printer, JSState* state) {
  JSBuilder* jsb = printer->jsb;
  StrSet seen;
  str_set_init(&seen, printer->arena);
  for(TransitionNode* t = state->node->event_transition; t != NULL; t = t->next) {
    const char* event = printer_event_name(t);
    if(event == NULL || !str_set_add(&seen, event)) {
      continue;
    }
    bool has_event = printer_handler_takes_event(printer, state, t);
    js_builder_open(jsb, "function %s(%s) {", printer_handler_name(printer, state, event), has_event ? "event" : "");
    printer_print_event_handler(printer, state, t, has_event);
    js_builder_close(jsb, "}");
  }
}

void printer_print_state_functions(Printer* printer, JSState* state) {
  for(; state != NULL; state = state->next) {
    if(state->flags & STATE_NEEDS_ENTER) {
      print_enter(printer, state);
    }
    if(state->flags & STATE_NEEDS_EXIT) {
      print_exit(printer, state);
    }
    if(state->flags & STATE_NEEDS_ALWAYS) {
      print_always(printer, state);
    }
    if(state->child != NULL && !state->top_final) {
      print_handlers(printer, state);
    }
    printer_print_state_functions(printer, state->child);
  }
}

// One entry in a leaf state's table: the leaf's own handler for an event,
// or the handler of the ancestor that has the event.
typedef struct Entry {
  const char* event;
  TransitionNode* own;
  JSState* ancestor;
} Entry;

static size_t count_leaves(JSState* state) {
  size_t count = 0;
  for(; state != NULL; state = state->next) {
    count += state->child != NULL ? count_leaves(state->child) : 1;
  }
  return count;
}

static size_t count_transitions(JSState* state) {
  size_t count = 0;
  for(TransitionNode* t = state->node->event_transition; t != NULL; t = t->next) {
    count++;
  }
  return count;
}

static void print_leaf(Printer* printer, JSState* leaf, bool last) {
  JSBuilder* jsb = printer->jsb;

  size_t capacity = count_transitions(leaf);
  for(JSState* ancestor = leaf->parent; ancestor != NULL; ancestor = ancestor->parent) {
    capacity += count_transitions(ancestor);
  }
  Entry* entries = arena_alloc_array(printer->arena, capacity > 0 ? capacity : 1, sizeof(Entry));
  size_t count = 0;
  StrSet seen;
  str_set_init(&seen, printer->arena);
  // A final state takes no events.
  if(!leaf->top_final) {
    for(TransitionNode* t = leaf->node->event_transition; t != NULL; t = t->next) {
      const char* event = printer_event_name(t);
      if(event != NULL && str_set_add(&seen, event)) {
        entries[count++] = (Entry){ .event = event, .own = t };
      }
    }
    for(JSState* ancestor = leaf->parent; ancestor != NULL; ancestor = ancestor->parent) {
      for(TransitionNode* t = ancestor->node->event_transition; t != NULL; t = t->next) {
        const char* event = printer_event_name(t);
        if(event != NULL && str_set_add(&seen, event)) {
          entries[count++] = (Entry){ .event = event, .ancestor = ancestor };
        }
      }
    }
  }

  const char* key = strchr(leaf->path, '.') != NULL ? printer_concat(printer, printer_concat(printer, "'", leaf->path), "'") : leaf->path;
  if(count == 0) {
    js_builder_line(jsb, "%s: {}%s", key, last ? "" : ",");
    return;
  }
  js_builder_open(jsb, "%s: {", key);
  for(size_t i = 0; i < count; i++) {
    Entry* entry = &entries[i];
    const char* comma = i + 1 < count ? "," : "";
    if(entry->own != NULL) {
      bool has_event = printer_handler_takes_event(printer, leaf, entry->own);
      js_builder_open(jsb, "%s(%s) {", entry->event, has_event ? "event" : "");
      printer_print_event_handler(printer, leaf, entry->own, has_event);
      js_builder_close(jsb, "}%s", comma);
    } else {
      js_builder_line(jsb, "%s: %s%s", entry->event, printer_handler_name(printer, entry->ancestor, entry->event), comma);
    }
  }
  js_builder_close(jsb, "}%s", last ? "" : ",");
}

static void print_leaves(Printer* printer, JSState* state, size_t* remaining) {
  for(; state != NULL; state = state->next) {
    if(state->child != NULL) {
      print_leaves(printer, state->child, remaining);
    } else {
      (*remaining)--;
      print_leaf(printer, state, *remaining == 0);
    }
  }
}

void printer_print_states_table(Printer* printer, JSMachine* machine) {
  JSBuilder* jsb = printer->jsb;
  size_t remaining = count_leaves(machine->states);
  js_builder_open(jsb, "const states = {");
  print_leaves(printer, machine->states, &remaining);
  js_builder_close(jsb, "};");
}
