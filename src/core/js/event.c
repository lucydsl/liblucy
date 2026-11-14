#include "printer.h"

#include <stdbool.h>
#include <string.h>

/*
 * The events a machine declares, and the warnings about a machine whose
 * states don't keep to them. A machine that declares no events is not
 * checked, except for what it sends to a machine that does.
 */

void printer_build_events(Printer* printer, JSMachine* machine) {
  JSEvent** tail = &machine->events;
  for(Node* node = machine->node->node.child; node != NULL; node = node->next) {
    if(node->type != NODE_EVENT) {
      continue;
    }
    EventNode* event_node = (EventNode*)node;
    JSEvent* event = ARENA_NEW(printer->arena, JSEvent);
    event->name = event_node->name;
    event->type = event_node->type;
    if(event->type != NULL) {
      str_set_add(&printer->type_names, event->type);
    }
    *tail = event;
    tail = &event->next;
  }
}

JSEvent* printer_find_event(JSMachine* machine, const char* name) {
  for(JSEvent* event = machine->events; event != NULL; event = event->next) {
    if(strcmp(event->name, name) == 0) {
      return event;
    }
  }
  return NULL;
}

bool printer_is_type_name(Printer* printer, const char* name) {
  return str_set_contains(&printer->type_names, name);
}

// The expression an action stands for, like printer_action_expression but
// without its warning, which printing the action gives.
static Expression* action_expression(Printer* printer, TransitionAction* action) {
  if(action->name == NULL) {
    return action->expression;
  }
  AssignmentNode* binding = str_map_get(printer->bindings, action->name);
  return binding != NULL ? binding->value : NULL;
}

// Called for each action of a machine: state is the state it is in, and
// event the declared event of the machine that it runs on, or NULL.
typedef void (*ActionVisitor)(Printer*, JSMachine*, JSState* state, JSEvent* event, Expression*, void* data);

typedef struct Visit {
  Printer* printer;
  JSMachine* machine;
  ActionVisitor visitor;
  void* data;
} Visit;

static void visit_actions(Visit* visit, JSState* state, JSEvent* event, TransitionAction* action) {
  for(; action != NULL; action = action->next) {
    Expression* expression = action_expression(visit->printer, action);
    if(expression != NULL) {
      visit->visitor(visit->printer, visit->machine, state, event, expression, visit->data);
    }
  }
}

static void visit_transitions(Visit* visit, JSState* state, TransitionNode* transition, bool named_events) {
  for(; transition != NULL; transition = transition->next) {
    const char* name = named_events ? printer_event_name(transition) : NULL;
    JSEvent* event = name != NULL ? printer_find_event(visit->machine, name) : NULL;
    visit_actions(visit, state, event, transition->action);
  }
}

static void visit_states(Visit* visit, JSState* state) {
  for(; state != NULL; state = state->next) {
    StateNode* node = state->node;
    if(node->entry != NULL) {
      visit_actions(visit, state, NULL, node->entry->action);
    }
    if(node->exit != NULL) {
      visit_actions(visit, state, NULL, node->exit->action);
    }
    visit_transitions(visit, state, node->event_transition, true);
    visit_transitions(visit, state, node->immediate_transition, false);
    visit_transitions(visit, state, node->delay_transition, false);
    if(node->invoke != NULL) {
      visit_transitions(visit, state, node->invoke->event_transition, false);
    }
    visit_states(visit, state->child);
  }
}

static void visit_machine(Printer* printer, JSMachine* machine, ActionVisitor visitor, void* data) {
  Visit visit = { .printer = printer, .machine = machine, .visitor = visitor, .data = data };
  visit_states(&visit, machine->states);
}

// What a context key holds when it isn't always the same machine of this
// file: something from the options, or one machine here and another there.
static JSMachine unknown_actor;

// Record, by context key, the machine each assign(key, spawn(name)) spawns.
static void collect_spawn(Printer* printer, JSMachine* machine, JSState* state, JSEvent* event, Expression* expression, void* data) {
  (void)machine;
  (void)state;
  (void)event;
  StrMap* actors = data;
  if(expression->type != EXPRESSION_ASSIGN) {
    return;
  }
  AssignExpression* assign = (AssignExpression*)expression;
  JSMachine* spawned = &unknown_actor;
  if(assign->value != NULL && assign->value->type == EXPRESSION_SPAWN) {
    Expression* target = ((SpawnExpression*)assign->value)->target;
    if(target->type == EXPRESSION_IDENTIFIER) {
      JSMachine* found = printer_find_machine(printer, ((IdentifierExpression*)target)->name);
      if(found != NULL) {
        spawned = found;
      }
    }
  }
  JSMachine* before = str_map_get(actors, assign->key);
  str_map_set(actors, assign->key, before == NULL || before == spawned ? spawned : &unknown_actor);
}

static void check_action(Printer* printer, JSMachine* machine, JSState* state, JSEvent* event, Expression* expression, void* data) {
  StrMap* actors = data;
  if(expression->type == EXPRESSION_ASSIGN) {
    AssignExpression* assign = (AssignExpression*)expression;
    if(assign->value == NULL && event != NULL && event->type == NULL) {
      printer_warnf(printer, LUCY_STREAM_STDOUT,
        "assign(%s) in state [%s] stores the data of event [%s], which is declared without any.\n",
        assign->key, state->path, event->name);
    }
  } else if(expression->type == EXPRESSION_SEND) {
    SendExpression* send = (SendExpression*)expression;
    if(send->actor == NULL || send->event == NULL) {
      return;
    }
    JSMachine* actor = str_map_get(actors, send->actor);
    if(actor != NULL && actor != &unknown_actor && actor != machine && actor->events != NULL &&
       printer_find_event(actor, send->event) == NULL) {
      printer_warnf(printer, LUCY_STREAM_STDOUT,
        "send(%s, %s) in state [%s] sends an event that machine [%s] does not declare.\n",
        send->actor, send->event, state->path, actor->name);
    }
  }
}

// Warn about each event the states handle that the machine doesn't
// declare, once each, and note in handled the ones it does.
static void check_handled(Printer* printer, JSMachine* machine, JSState* state, StrSet* handled, StrSet* warned) {
  for(; state != NULL; state = state->next) {
    for(TransitionNode* t = state->node->event_transition; t != NULL; t = t->next) {
      const char* name = printer_event_name(t);
      if(name == NULL) {
        continue;
      }
      if(printer_find_event(machine, name) != NULL) {
        str_set_add(handled, name);
      } else if(str_set_add(warned, name)) {
        printer_warnf(printer, LUCY_STREAM_STDOUT,
          "Event [%s] in state [%s] is not declared by machine [%s].\n", name, state->path, machine->name);
      }
    }
    check_handled(printer, machine, state->child, handled, warned);
  }
}

void printer_check_events(Printer* printer, JSMachine* machine) {
  if(machine->events != NULL) {
    StrSet handled;
    StrSet warned;
    str_set_init(&handled, printer->arena);
    str_set_init(&warned, printer->arena);
    check_handled(printer, machine, machine->states, &handled, &warned);
    for(JSEvent* event = machine->events; event != NULL; event = event->next) {
      if(!str_set_contains(&handled, event->name)) {
        printer_warnf(printer, LUCY_STREAM_STDOUT,
          "Event [%s] is declared by machine [%s], but none of its states handles it.\n", event->name, machine->name);
      }
    }
  }

  StrMap* actors = str_map_create(printer->arena);
  visit_machine(printer, machine, collect_spawn, actors);
  visit_machine(printer, machine, check_action, actors);
}
