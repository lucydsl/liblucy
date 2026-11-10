#include "printer.h"

#include <stdbool.h>
#include <string.h>

// The name of the function for owner's transitions on event, when owner
// is a compound state: redOnTimer.
static const char* handler_name(Printer* printer, JSState* owner, const char* event) {
  return printer_concat(printer, printer_concat(printer, owner->var, "On"), printer_capitalize(printer, event));
}

const char* printer_handler_name(Printer* printer, JSState* owner, const char* event) {
  return handler_name(printer, owner, event);
}

bool printer_handler_takes_event(Printer* printer, JSState* owner, TransitionNode* first) {
  bool all_guarded = true;
  for(TransitionNode* t = first; t != NULL; t = t->link) {
    if(printer_transition_takes_event(printer, owner, t)) {
      return true;
    }
    if(t->guard == NULL) {
      all_guarded = false;
      break;
    }
  }
  if(all_guarded) {
    // Falls through to the ancestor's handler, which may take the event.
    const char* event = printer_event_name(first);
    JSState* ancestor = event != NULL ? printer_find_ancestor_handler(owner, event) : NULL;
    if(ancestor != NULL) {
      return printer_handler_takes_event(printer, ancestor, printer_find_event_transition(ancestor, event));
    }
  }
  return false;
}

void printer_print_exit_call(Printer* printer, JSState* owner, bool has_event) {
  if(!(owner->flags & STATE_NEEDS_EXIT)) {
    return;
  }
  bool with_event = has_event && (owner->flags & STATE_EXIT_TAKES_EVENT);
  js_builder_line(printer->jsb, "exit%s(%s);", owner->ident, with_event ? "event" : "");
}

// The function that enters state, given to its parent's enter function
// when a transition goes straight to a nested state: enterIdleErrored, or
// an arrow that passes the next level on in turn.
static void add_enter_child(Printer* printer, JSState* state, JSState* target) {
  JSBuilder* jsb = printer->jsb;
  if(state == target) {
    js_builder_add_format(jsb, "enter%s", state->ident);
    return;
  }
  JSState* child = target;
  while(child->parent != state) {
    child = child->parent;
  }
  js_builder_add_format(jsb, "event => enter%s(event, ", state->ident);
  add_enter_child(printer, child, target);
  js_builder_add_str(jsb, ")");
}

static void add_dest_text(JSBuilder* jsb, Expression* dest) {
  while(dest->type == EXPRESSION_MEMBER) {
    MemberExpression* member = (MemberExpression*)dest;
    js_builder_add_format(jsb, "%s.", member->owner);
    dest = member->property;
  }
  if(dest->type == EXPRESSION_IDENTIFIER) {
    js_builder_add_str(jsb, ((IdentifierExpression*)dest)->name);
  }
}

void printer_print_enter_target(Printer* printer, JSState* owner, Expression* dest, bool has_event) {
  JSBuilder* jsb = printer->jsb;
  JSState* target;
  JSState* point = printer_enter_point(printer, owner, dest, &target);
  if(point == NULL) {
    // Already warned about; print the name as written.
    js_builder_add_indent(jsb);
    js_builder_add_str(jsb, "machine.state = '");
    add_dest_text(jsb, dest);
    js_builder_add_str(jsb, "';\n");
    return;
  }
  if(!(point->flags & STATE_NEEDS_ENTER)) {
    js_builder_line(jsb, "machine.state = '%s';", printer_leaf(point)->path);
    if(point->top_final) {
      js_builder_line(jsb, "machine.done = true;");
    }
    return;
  }
  js_builder_add_indent(jsb);
  js_builder_add_format(jsb, "enter%s(", point->ident);
  if(has_event && (point->flags & STATE_ENTER_TAKES_EVENT)) {
    js_builder_add_str(jsb, "event");
  }
  if(target != point) {
    JSState* child = target;
    while(child->parent != point) {
      child = child->parent;
    }
    js_builder_add_str(jsb, ", ");
    add_enter_child(printer, child, target);
  }
  js_builder_add_str(jsb, ");\n");
}

bool printer_immediate_takes_event(Printer* printer, JSState* state) {
  for(TransitionNode* t = state->node->immediate_transition; t != NULL; t = t->next) {
    if(printer_transition_takes_event(printer, state, t)) {
      return true;
    }
  }
  return false;
}

void printer_print_transition(Printer* printer, JSState* owner, TransitionNode* transition, bool has_event) {
  if(transition->dest != NULL) {
    printer_print_exit_call(printer, owner, has_event);
  }
  for(TransitionAction* action = transition->action; action != NULL; action = action->next) {
    printer_print_action(printer, action);
  }
  if(transition->dest != NULL) {
    printer_print_enter_target(printer, owner, transition->dest, has_event);
  } else if(owner->flags & STATE_NEEDS_ALWAYS) {
    bool with_event = has_event && printer_immediate_takes_event(printer, owner);
    js_builder_line(printer->jsb, "always%s(%s);", owner->ident, with_event ? "event" : "");
  }
}

// `if(guards) {` or `} else if(guards) {`, then one level in.
static void open_branch(Printer* printer, bool first, TransitionGuard* guard) {
  JSBuilder* jsb = printer->jsb;
  if(!first) {
    jsb->depth--;
  }
  js_builder_add_indent(jsb);
  js_builder_add_str(jsb, first ? "if(" : "} else if(");
  printer_add_guards(printer, guard);
  js_builder_add_str(jsb, ") {\n");
  jsb->depth++;
}

// The transitions of a chain, in order, as an if/else chain. The first
// whose guards pass is taken; one with no guards ends the chain. When
// every transition is guarded and fallthrough names a state, its handler
// for the event is called when none passes.
static void print_chain(Printer* printer, JSState* owner, TransitionNode* first, bool linked,
  bool has_event, JSState* fallthrough, const char* event) {
  JSBuilder* jsb = printer->jsb;
  bool opened = false;
  bool closed = false;
  for(TransitionNode* t = first; t != NULL; t = linked ? t->link : t->next) {
    if(t->guard != NULL) {
      open_branch(printer, !opened, t->guard);
      opened = true;
      printer_print_transition(printer, owner, t, has_event);
    } else {
      if(opened) {
        js_builder_close(jsb, "} else {");
        jsb->depth++;
        printer_print_transition(printer, owner, t, has_event);
        js_builder_close(jsb, "}");
        closed = true;
      } else {
        printer_print_transition(printer, owner, t, has_event);
      }
      break;
    }
  }
  if(opened && !closed) {
    if(fallthrough != NULL) {
      js_builder_close(jsb, "} else {");
      jsb->depth++;
      bool with_event = has_event &&
        printer_handler_takes_event(printer, fallthrough, printer_find_event_transition(fallthrough, event));
      js_builder_line(jsb, "%s(%s);", handler_name(printer, fallthrough, event), with_event ? "event" : "");
    }
    js_builder_close(jsb, "}");
  }
}

void printer_print_event_handler(Printer* printer, JSState* owner, TransitionNode* transition, bool has_event) {
  const char* event = printer_event_name(transition);
  JSState* fallthrough = NULL;
  // Transitions of an invoke (done, error) don't fall through to anything.
  if(event != NULL && transition->node.parent != NULL && transition->node.parent->type == NODE_STATE) {
    fallthrough = printer_find_ancestor_handler(owner, event);
  }
  print_chain(printer, owner, transition, true, has_event, fallthrough, event);
}

void printer_print_immediate_transitions(Printer* printer, JSState* owner, bool has_event) {
  print_chain(printer, owner, owner->node->immediate_transition, false, has_event, NULL, NULL);
}
