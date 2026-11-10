#include "printer.h"

#include <stdbool.h>
#include <string.h>

// The states of machine_node, nested in parent (NULL at the top level).
// Nested machines inside the states are built too.
static JSState* build_states(Printer* printer, JSMachine* machine, JSState* parent, MachineNode* machine_node) {
  JSState* first = NULL;
  JSState** tail = &first;
  for(Node* node = machine_node->node.child; node != NULL; node = node->next) {
    if(node->type != NODE_STATE) {
      continue;
    }
    StateNode* state_node = (StateNode*)node;
    JSState* state = ARENA_NEW(printer->arena, JSState);
    state->node = state_node;
    state->machine = machine;
    state->parent = parent;
    state->name = printer_span(printer, state_node->name);
    state->top_final = state_node->final && parent == NULL;
    if(parent == NULL) {
      state->path = state->name;
      state->ident = printer_capitalize(printer, state->name);
      state->var = state->name;
    } else {
      const char* capitalized = printer_capitalize(printer, state->name);
      state->path = printer_concat(printer, printer_concat(printer, parent->path, "."), state->name);
      state->ident = printer_concat(printer, parent->ident, capitalized);
      state->var = printer_concat(printer, parent->var, capitalized);
    }

    for(TransitionNode* t = state_node->delay_transition; t != NULL; t = t->next) {
      state->delay_count++;
    }

    // The first nested machine makes this a compound state. The parser
    // accepts more than one, which XState could not express either.
    for(Node* child = node->child; child != NULL; child = child->next) {
      if(child->type == NODE_MACHINE) {
        state->nested = (MachineNode*)child;
        break;
      }
    }

    *tail = state;
    tail = &state->next;
  }

  // The children can only be built once every top-level machine is known,
  // as invoke(name) may name one; see printer_build_model.
  for(JSState* state = first; state != NULL; state = state->next) {
    if(state->nested != NULL) {
      state->child = build_states(printer, machine, state, state->nested);
    }
  }
  return first;
}

// The state machine_node marks `initial`, or the first state. NULL for a
// machine with no states.
static JSState* find_initial(Printer* printer, JSState* states, MachineNode* machine_node) {
  if(machine_node->has_initial) {
    const char* name = printer_span(printer, machine_node->initial);
    for(JSState* state = states; state != NULL; state = state->next) {
      if(strcmp(state->name, name) == 0) {
        return state;
      }
    }
  }
  return states;
}

static void set_initials(Printer* printer, JSState* states, MachineNode* machine_node, JSMachine* machine) {
  JSState* initial = find_initial(printer, states, machine_node);
  if(machine != NULL) {
    machine->initial = initial;
  }
  for(JSState* state = states; state != NULL; state = state->next) {
    if(state->child != NULL) {
      state->initial = find_initial(printer, state->child, state->nested);
      set_initials(printer, state->child, state->nested, NULL);
    }
  }
}

// Record every `action name = ...` and `guard name = ...` under node.
static void collect_bindings(Printer* printer, Node* node) {
  for(; node != NULL; node = node->next) {
    if(node->type == NODE_ASSIGNMENT) {
      AssignmentNode* assignment = (AssignmentNode*)node;
      str_map_set(printer->bindings, assignment->binding_name, assignment);
    }
    collect_bindings(printer, node->child);
  }
}

JSMachine* printer_find_machine(Printer* printer, const char* name) {
  for(JSMachine* machine = printer->machines; machine != NULL; machine = machine->next) {
    if(machine->name != NULL && strcmp(machine->name, name) == 0) {
      return machine;
    }
  }
  return NULL;
}

static void set_invoke_kind(Printer* printer, JSState* state) {
  InvokeNode* invoke = state->node->invoke;
  if(invoke == NULL || invoke->expr == NULL || invoke->expr->ref == NULL) {
    return;
  }
  Expression* ref = invoke->expr->ref;
  if(ref->type == EXPRESSION_IDENTIFIER) {
    JSMachine* machine = printer_find_machine(printer, ((IdentifierExpression*)ref)->name);
    if(machine != NULL) {
      state->invoke_kind = INVOKE_MACHINE;
      state->invoked = machine;
      return;
    }
  }
  state->invoke_kind = INVOKE_PROMISE;
}

static JSState* find_sibling(JSState* states, const char* name) {
  for(JSState* state = states; state != NULL; state = state->next) {
    if(strcmp(state->name, name) == 0) {
      return state;
    }
  }
  return NULL;
}

JSState* printer_resolve_target(Printer* printer, JSState* source, Expression* dest) {
  (void)printer;
  JSState* siblings = source->parent == NULL ? source->machine->states : source->parent->child;
  Expression* expression = dest;
  JSState* state = NULL;
  while(true) {
    const char* name;
    Expression* rest = NULL;
    if(expression->type == EXPRESSION_MEMBER) {
      MemberExpression* member = (MemberExpression*)expression;
      name = member->owner;
      rest = member->property;
    } else if(expression->type == EXPRESSION_IDENTIFIER) {
      name = ((IdentifierExpression*)expression)->name;
    } else {
      return NULL;
    }
    state = find_sibling(siblings, name);
    if(state == NULL || rest == NULL) {
      return state;
    }
    siblings = state->child;
    expression = rest;
  }
}

// dest as text, for a warning about it.
static void add_dest_text(StrBuilder* sb, Expression* dest) {
  while(dest->type == EXPRESSION_MEMBER) {
    MemberExpression* member = (MemberExpression*)dest;
    str_builder_add_str(sb, member->owner);
    str_builder_add_char(sb, '.');
    dest = member->property;
  }
  if(dest->type == EXPRESSION_IDENTIFIER) {
    str_builder_add_str(sb, ((IdentifierExpression*)dest)->name);
  }
}

// Whether entering state does something of its own, beyond entering a
// child: entry actions, timers, an invoke, immediate transitions.
static bool has_own_enter_work(JSState* state) {
  StateNode* node = state->node;
  return node->entry != NULL || state->delay_count > 0 || node->invoke != NULL ||
         node->immediate_transition != NULL;
}

// Check a transition's target. A dotted target, `idle.errored`, is entered
// through the enter functions of each state on the way when one of those
// states has entry work of its own: every state on the way gets one, and
// the ones above the target take the child to enter as a parameter (with
// their initial child as the default, so that child needs an enter
// function too). When none has, the target is entered directly.
static void check_target(Printer* printer, JSState* source, TransitionNode* transition) {
  if(transition->dest == NULL) {
    return;
  }
  JSState* target = printer_resolve_target(printer, source, transition->dest);
  if(target == NULL) {
    StrBuilder* sb = str_builder_create();
    str_builder_add_str(sb, "Unknown state [");
    add_dest_text(sb, transition->dest);
    str_builder_add_str(sb, "] in state [");
    str_builder_add_str(sb, source->path);
    str_builder_add_str(sb, "]. The transition's code will fail at run time.\n");
    printer_warn(printer, LUCY_STREAM_STDOUT, str_builder_peek(sb));
    str_builder_destroy(sb);
    return;
  }
  bool chain_has_work = false;
  for(JSState* state = target->parent; state != NULL && state != source->parent; state = state->parent) {
    if(has_own_enter_work(state)) {
      chain_has_work = true;
    }
  }
  if(!chain_has_work) {
    return;
  }
  target->flags |= STATE_NEEDS_ENTER;
  for(JSState* state = target->parent; state != source->parent; state = state->parent) {
    state->flags |= STATE_NEEDS_ENTER | STATE_ENTER_TAKES_CHILD | STATE_ENTER_TAKES_EVENT;
    if(state->initial != NULL) {
      state->initial->flags |= STATE_NEEDS_ENTER;
    }
  }
}

static void check_targets(Printer* printer, JSState* states) {
  for(JSState* state = states; state != NULL; state = state->next) {
    StateNode* node = state->node;
    for(TransitionNode* t = node->event_transition; t != NULL; t = t->next) {
      check_target(printer, state, t);
    }
    for(TransitionNode* t = node->immediate_transition; t != NULL; t = t->next) {
      check_target(printer, state, t);
    }
    for(TransitionNode* t = node->delay_transition; t != NULL; t = t->next) {
      check_target(printer, state, t);
    }
    if(node->invoke != NULL) {
      for(TransitionNode* t = node->invoke->event_transition; t != NULL; t = t->next) {
        check_target(printer, state, t);
      }
    }
    if(state->top_final && (node->event_transition != NULL || node->immediate_transition != NULL ||
                             node->delay_transition != NULL || node->invoke != NULL)) {
      printer_warnf(printer, LUCY_STREAM_STDOUT,
        "State [%s] is final, so its transitions are never taken.\n", state->path);
    }
    check_targets(printer, state->child);
  }
}

JSState* printer_leaf(JSState* state) {
  while(state->initial != NULL) {
    state = state->initial;
  }
  return state;
}

const char* printer_event_name(TransitionNode* transition) {
  Expression* event = transition->event;
  if(event == NULL) {
    return NULL;
  }
  if(event->type == EXPRESSION_ON) {
    return ((OnExpression*)event)->name;
  }
  if(event->type == EXPRESSION_IDENTIFIER) {
    return ((IdentifierExpression*)event)->name;
  }
  return NULL;
}

TransitionNode* printer_find_event_transition(JSState* state, const char* event) {
  for(TransitionNode* t = state->node->event_transition; t != NULL; t = t->next) {
    const char* name = printer_event_name(t);
    if(name != NULL && strcmp(name, event) == 0) {
      return t;
    }
  }
  return NULL;
}

JSState* printer_find_ancestor_handler(JSState* state, const char* event) {
  for(JSState* ancestor = state->parent; ancestor != NULL; ancestor = ancestor->parent) {
    if(printer_find_event_transition(ancestor, event) != NULL) {
      return ancestor;
    }
  }
  return NULL;
}

// Whether any transition in the list has no target. Such a transition
// runs its actions and stays, so the state's immediate transitions have
// to be looked at again afterwards.
static bool any_targetless(TransitionNode* transition) {
  for(; transition != NULL; transition = transition->next) {
    if(transition->dest == NULL) {
      return true;
    }
  }
  return false;
}

// Whether state can be left by something other than its own timers and
// invoke while they are pending: an event of its own or of a state it is
// nested in, an immediate transition, or a timer of a state it is nested
// in.
static bool can_leave_early(JSState* state) {
  StateNode* node = state->node;
  if(node->event_transition != NULL || node->immediate_transition != NULL) {
    return true;
  }
  for(JSState* ancestor = state->parent; ancestor != NULL; ancestor = ancestor->parent) {
    StateNode* above = ancestor->node;
    if(above->event_transition != NULL || above->immediate_transition != NULL ||
       above->delay_transition != NULL || above->invoke != NULL) {
      return true;
    }
  }
  return false;
}

// Leaving state has to clear its timers when something else can leave it
// while one is pending: another timer, its invoke, or anything else.
static bool timers_need_exit(JSState* state) {
  return state->delay_count > 1 || state->node->invoke != NULL || can_leave_early(state);
}

// Likewise its pending promise.
static bool promise_needs_exit(JSState* state) {
  return state->delay_count > 0 || can_leave_early(state);
}

JSState* printer_enter_point(Printer* printer, JSState* owner, Expression* dest, JSState** target_out) {
  JSState* target = printer_resolve_target(printer, owner, dest);
  if(target_out != NULL) {
    *target_out = target;
  }
  if(target == NULL) {
    return NULL;
  }
  // The state at owner's level that the target is, or is inside.
  JSState* top = target;
  while(top->parent != owner->parent) {
    top = top->parent;
  }
  // A nested target is entered through its ancestors only when they have
  // entry work of their own (see check_target); otherwise directly.
  if(target != top && !(top->flags & STATE_ENTER_TAKES_CHILD)) {
    return target;
  }
  return top;
}

bool printer_enter_takes_event(Printer* printer, JSState* owner, Expression* dest) {
  JSState* point = printer_enter_point(printer, owner, dest, NULL);
  if(point == NULL) {
    return false;
  }
  return (point->flags & STATE_NEEDS_ENTER) && (point->flags & STATE_ENTER_TAKES_EVENT);
}

static bool immediate_takes_event(Printer* printer, JSState* state) {
  for(TransitionNode* t = state->node->immediate_transition; t != NULL; t = t->next) {
    if(printer_transition_takes_event(printer, state, t)) {
      return true;
    }
  }
  return false;
}

bool printer_transition_takes_event(Printer* printer, JSState* owner, TransitionNode* transition) {
  if(transition->guard != NULL || printer_actions_take_event(printer, transition->action)) {
    return true;
  }
  if(transition->dest != NULL) {
    if((owner->flags & STATE_NEEDS_EXIT) && (owner->flags & STATE_EXIT_TAKES_EVENT)) {
      return true;
    }
    return printer_enter_takes_event(printer, owner, transition->dest);
  }
  return (owner->flags & STATE_NEEDS_ALWAYS) && immediate_takes_event(printer, owner);
}

static bool any_delay_named(JSState* state) {
  for(TransitionNode* t = state->node->delay_transition; t != NULL; t = t->next) {
    if(t->delay != NULL && t->delay->expression != NULL && t->delay->expression->ref != NULL) {
      return true;
    }
  }
  return false;
}

// The flags of state from what it has and from the flags its neighbours
// have so far. Returns true if anything changed.
static bool compute_flags(Printer* printer, JSState* state) {
  StateNode* node = state->node;
  unsigned flags = state->flags;
  TransitionAction* entry = node->entry != NULL ? node->entry->action : NULL;
  TransitionAction* exit = node->exit != NULL ? node->exit->action : NULL;

  if(entry != NULL || state->delay_count > 0 || state->invoke_kind != INVOKE_NONE ||
     node->immediate_transition != NULL) {
    flags |= STATE_NEEDS_ENTER;
  }
  if(exit != NULL || (state->delay_count > 0 && timers_need_exit(state)) ||
     (state->invoke_kind == INVOKE_PROMISE && promise_needs_exit(state)) ||
     state->invoke_kind == INVOKE_MACHINE) {
    flags |= STATE_NEEDS_EXIT;
  }
  if(printer_actions_take_event(printer, exit)) {
    flags |= STATE_EXIT_TAKES_EVENT;
  }
  if(node->immediate_transition != NULL &&
     (any_targetless(node->event_transition) || any_targetless(node->delay_transition) ||
       (node->invoke != NULL && any_targetless(node->invoke->event_transition)))) {
    flags |= STATE_NEEDS_ALWAYS;
  }
  for(JSState* child = state->child; child != NULL; child = child->next) {
    if(child->flags & STATE_NEEDS_EXIT) {
      flags |= STATE_NEEDS_EXIT;
      if(child->flags & STATE_EXIT_TAKES_EVENT) {
        flags |= STATE_EXIT_TAKES_EVENT;
      }
    }
  }
  if(state->initial != NULL && (state->initial->flags & STATE_NEEDS_ENTER)) {
    flags |= STATE_NEEDS_ENTER;
    if(state->initial->flags & STATE_ENTER_TAKES_EVENT) {
      flags |= STATE_ENTER_TAKES_EVENT;
    }
  }
  // Set state's own flags before asking about its immediate transitions,
  // whose exit call depends on them.
  bool changed = flags != state->flags;
  state->flags = flags;
  if(printer_actions_take_event(printer, entry) || any_delay_named(state) ||
     state->invoke_kind == INVOKE_PROMISE || immediate_takes_event(printer, state)) {
    flags |= STATE_ENTER_TAKES_EVENT;
  }
  changed = changed || flags != state->flags;
  state->flags = flags;
  return changed;
}

static bool compute_all_flags(Printer* printer, JSState* states) {
  bool changed = false;
  for(JSState* state = states; state != NULL; state = state->next) {
    if(state->child != NULL && compute_all_flags(printer, state->child)) {
      changed = true;
    }
    if(compute_flags(printer, state)) {
      changed = true;
    }
  }
  return changed;
}

static void set_invoke_kinds(Printer* printer, JSState* states) {
  for(JSState* state = states; state != NULL; state = state->next) {
    set_invoke_kind(printer, state);
    set_invoke_kinds(printer, state->child);
  }
}

void printer_build_model(Printer* printer) {
  printer->bindings = str_map_create(printer->arena);
  collect_bindings(printer, printer->program->body);

  JSMachine** tail = &printer->machines;
  for(Node* node = printer->program->body; node != NULL; node = node->next) {
    if(node->type != NODE_MACHINE) {
      continue;
    }
    MachineNode* machine_node = (MachineNode*)node;
    JSMachine* machine = ARENA_NEW(printer->arena, JSMachine);
    machine->node = machine_node;
    if(machine_node->has_name) {
      machine->name = printer_span(printer, machine_node->name);
      machine->factory = printer_concat(printer, "create", printer_capitalize(printer, machine->name));
    } else {
      machine->factory = "createMachine";
    }
    machine->states = build_states(printer, machine, NULL, machine_node);
    set_initials(printer, machine->states, machine_node, machine);
    for(JSState* state = machine->states; state != NULL; state = state->next) {
      if(state->top_final) {
        machine->has_final = true;
      }
    }
    *tail = machine;
    tail = &machine->next;
  }

  for(JSMachine* machine = printer->machines; machine != NULL; machine = machine->next) {
    set_invoke_kinds(printer, machine->states);
    check_targets(printer, machine->states);
    // The flags depend on each other across states (a state needs an exit
    // function if a child does, entering a target takes the event if its
    // enter function does), so go round until nothing changes. Each round
    // only adds flags, so this ends.
    while(compute_all_flags(printer, machine->states)) {
    }
  }
}
