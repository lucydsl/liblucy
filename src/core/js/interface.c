#include "printer.h"

#include <stdbool.h>
#include <string.h>

// What collecting a machine's interface works on.
typedef struct Build {
  Printer* printer;
  JSMachine* machine;
  JSInterface* interface;
} Build;

static void bag_init(JSSymbolBag* bag, Arena* arena) {
  bag->head = NULL;
  bag->tail = &bag->head;
  bag->by_name = str_map_create(arena);
}

static void bag_add(Build* build, JSSymbolBag* bag, const char* name, const char* key, const char* event) {
  JSSymbol* symbol = str_map_get(bag->by_name, name);
  if(symbol == NULL) {
    symbol = ARENA_NEW(build->printer->arena, JSSymbol);
    symbol->name = name;
    symbol->key = key;
    string_list_init(&symbol->events, build->printer->arena);
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

static void add_key(Build* build, const char* key) {
  if(str_set_add(&build->interface->key_set, key)) {
    string_list_append(&build->interface->keys, key);
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
static void add_event_data(Build* build, const char* key, const char* event) {
  JSEvent* declared = event != NULL ? printer_find_event(build->machine, event) : NULL;
  if(declared == NULL || declared->type == NULL) {
    return;
  }
  JSEventData** tail = &build->interface->event_data;
  while(*tail != NULL && strcmp((*tail)->key, key) != 0) {
    tail = &(*tail)->next;
  }
  if(*tail == NULL) {
    JSEventData* data = ARENA_NEW(build->printer->arena, JSEventData);
    data->key = key;
    string_list_init(&data->types, build->printer->arena);
    *tail = data;
  }
  if(!list_contains(&(*tail)->types, declared->type)) {
    string_list_append(&(*tail)->types, declared->type);
  }
}

static void collect_actions(Build* build, TransitionAction* action, const char* event) {
  for(; action != NULL; action = action->next) {
    Expression* expression = printer_action_expression(build->printer, action);
    if(expression == NULL) {
      continue;
    }
    if(expression->type == EXPRESSION_ASSIGN) {
      AssignExpression* assign = (AssignExpression*)expression;
      add_key(build, assign->key);
      if(assign->value == NULL) {
        add_event_data(build, assign->key, event);
      } else if(assign->value->type == EXPRESSION_SYMBOL) {
        bag_add(build, &build->interface->assigns, ((SymbolExpression*)assign->value)->name, assign->key, event);
      } else if(assign->value->type == EXPRESSION_SPAWN) {
        Expression* target = ((SpawnExpression*)assign->value)->target;
        if(target->type == EXPRESSION_SYMBOL) {
          bag_add(build, &build->interface->actors, ((SymbolExpression*)target)->name, NULL, event);
        }
      }
    } else if(expression->type == EXPRESSION_ACTION) {
      Expression* ref = ((ActionExpression*)expression)->ref;
      if(ref->type == EXPRESSION_SYMBOL) {
        bag_add(build, &build->interface->actions, ((SymbolExpression*)ref)->name, NULL, event);
      }
    } else if(expression->type == EXPRESSION_SYMBOL) {
      bag_add(build, &build->interface->actions, ((SymbolExpression*)expression)->name, NULL, event);
    }
  }
}

static void collect_guards(Build* build, TransitionGuard* guard, const char* event) {
  for(; guard != NULL; guard = guard->next) {
    Expression* ref = NULL;
    if(guard->name != NULL) {
      AssignmentNode* binding = str_map_get(build->printer->bindings, guard->name);
      ref = binding != NULL ? binding->value : NULL;
    } else if(guard->expression != NULL) {
      ref = guard->expression->ref;
    }
    if(ref != NULL && ref->type == EXPRESSION_SYMBOL) {
      bag_add(build, &build->interface->guards, ((SymbolExpression*)ref)->name, NULL, event);
    }
  }
}

static void collect_transitions(Build* build, TransitionNode* transition, bool named_events) {
  for(; transition != NULL; transition = transition->next) {
    const char* event = named_events ? printer_event_name(transition) : NULL;
    if(named_events && event != NULL && str_set_add(&build->interface->event_set, event)) {
      string_list_append(&build->interface->events, event);
    }
    collect_guards(build, transition->guard, event);
    collect_actions(build, transition->action, event);
  }
}

static void collect_states(Build* build, JSState* state) {
  for(; state != NULL; state = state->next) {
    StateNode* node = state->node;
    if(node->entry != NULL) {
      collect_actions(build, node->entry->action, NULL);
    }
    if(node->exit != NULL) {
      collect_actions(build, node->exit->action, NULL);
    }
    collect_transitions(build, node->event_transition, true);
    collect_transitions(build, node->immediate_transition, false);
    if(node->delay_transition != NULL) {
      build->interface->has_delay = true;
    }
    for(TransitionNode* t = node->delay_transition; t != NULL; t = t->next) {
      DelayExpression* delay = t->delay != NULL ? t->delay->expression : NULL;
      if(delay != NULL && delay->ref != NULL && delay->ref->type == EXPRESSION_SYMBOL) {
        bag_add(build, &build->interface->delays, ((SymbolExpression*)delay->ref)->name, NULL, NULL);
      }
    }
    collect_transitions(build, node->delay_transition, false);
    if(node->invoke != NULL) {
      build->interface->has_done = true;
      for(TransitionNode* t = node->invoke->event_transition; t != NULL; t = t->next) {
        const char* name = printer_event_name(t);
        if(name != NULL && strcmp(name, "error") == 0) {
          build->interface->has_error = true;
        }
      }
      Expression* ref = node->invoke->expr != NULL ? node->invoke->expr->ref : NULL;
      if(ref != NULL && ref->type == EXPRESSION_SYMBOL) {
        bag_add(build, &build->interface->services, ((SymbolExpression*)ref)->name, NULL, NULL);
      }
      collect_transitions(build, node->invoke->event_transition, false);
    }
    collect_states(build, state->child);
  }
}

void printer_build_interface(Printer* printer, JSMachine* machine) {
  Arena* arena = printer->arena;
  JSInterface* interface = ARENA_NEW(arena, JSInterface);
  bag_init(&interface->actions, arena);
  bag_init(&interface->assigns, arena);
  bag_init(&interface->guards, arena);
  bag_init(&interface->delays, arena);
  bag_init(&interface->services, arena);
  bag_init(&interface->actors, arena);
  string_list_init(&interface->events, arena);
  str_set_init(&interface->event_set, arena);
  string_list_init(&interface->keys, arena);
  str_set_init(&interface->key_set, arena);
  machine->interface = interface;
  Build build = { .printer = printer, .machine = machine, .interface = interface };
  collect_states(&build, machine->states);
}

bool printer_interface_needs_options(const JSInterface* interface) {
  return interface->actions.head != NULL || interface->assigns.head != NULL ||
         interface->guards.head != NULL || interface->delays.head != NULL ||
         interface->services.head != NULL || interface->actors.head != NULL;
}

// Add the names of bag's symbols to text: `guards.ok, guards.busy`.
static void add_needs(StrBuilder* text, const JSSymbolBag* bag, const char* option) {
  for(JSSymbol* symbol = bag->head; symbol != NULL; symbol = symbol->next) {
    if(str_builder_peek(text)[0] != '\0') {
      str_builder_add_str(text, ", ");
    }
    str_builder_add_str(text, option);
    str_builder_add_str(text, ".");
    str_builder_add_str(text, symbol->name);
  }
}

// Warn, once for each machine, that machine creates child, when child
// needs options: a machine created by another gets none.
static void check_child(Printer* printer, JSMachine* machine, JSMachine* child, const char* how, StrSet* warned) {
  JSInterface* needs = child->interface;
  if(!printer_interface_needs_options(needs) || !str_set_add(warned, child->name)) {
    return;
  }
  StrBuilder* text = str_builder_create();
  add_needs(text, &needs->actions, "actions");
  add_needs(text, &needs->assigns, "assigns");
  add_needs(text, &needs->delays, "delays");
  add_needs(text, &needs->guards, "guards");
  add_needs(text, &needs->services, "services");
  add_needs(text, &needs->actors, "services");
  printer_warnf(printer, LUCY_STREAM_STDOUT,
    "Machine [%s] %s machine [%s], which needs options that nothing supplies to it (%s), so it will throw when it uses them.\n",
    machine->name != NULL ? machine->name : "(unnamed)", how, child->name, str_builder_peek(text));
  str_builder_destroy(text);
}

static void check_spawns(Printer* printer, JSMachine* machine, TransitionAction* action, StrSet* warned) {
  for(; action != NULL; action = action->next) {
    AssignExpression* assign = printer_action_spawn(printer, action);
    if(assign == NULL) {
      continue;
    }
    Expression* target = ((SpawnExpression*)assign->value)->target;
    if(target->type != EXPRESSION_IDENTIFIER) {
      continue;
    }
    JSMachine* child = printer_find_machine(printer, ((IdentifierExpression*)target)->name);
    if(child != NULL) {
      check_child(printer, machine, child, "spawns", warned);
    }
  }
}

static void check_transitions(Printer* printer, JSMachine* machine, TransitionNode* transition, StrSet* warned) {
  for(; transition != NULL; transition = transition->next) {
    check_spawns(printer, machine, transition->action, warned);
  }
}

static void check_states(Printer* printer, JSMachine* machine, JSState* state, StrSet* warned) {
  for(; state != NULL; state = state->next) {
    StateNode* node = state->node;
    if(node->entry != NULL) {
      check_spawns(printer, machine, node->entry->action, warned);
    }
    if(node->exit != NULL) {
      check_spawns(printer, machine, node->exit->action, warned);
    }
    check_transitions(printer, machine, node->event_transition, warned);
    check_transitions(printer, machine, node->immediate_transition, warned);
    check_transitions(printer, machine, node->delay_transition, warned);
    if(node->invoke != NULL) {
      check_transitions(printer, machine, node->invoke->event_transition, warned);
    }
    if(state->invoke_kind == INVOKE_MACHINE) {
      check_child(printer, machine, state->invoked, "invokes", warned);
    }
    check_states(printer, machine, state->child, warned);
  }
}

void printer_check_interfaces(Printer* printer, JSMachine* machine) {
  StrSet warned;
  str_set_init(&warned, printer->arena);
  check_states(printer, machine, machine->states, &warned);
}
