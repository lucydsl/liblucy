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
        SpawnExpression* spawn = (SpawnExpression*)assign->value;
        Expression* target = spawn->target;
        if(target->type == EXPRESSION_SYMBOL) {
          bag_add(build, &build->interface->actors, ((SymbolExpression*)target)->name, NULL, event);
        }
        // The values it gives: a :symbol is in the assigns option, as the
        // value of an assign is, and parent is the machine's own.
        for(SpawnField* field = spawn->fields; field != NULL; field = field->next) {
          if(field->symbol) {
            // Code, which printer_forward_symbols sees to.
            continue;
          }
          if(field->value->type == EXPRESSION_SYMBOL) {
            bag_add(build, &build->interface->assigns, ((SymbolExpression*)field->value)->name, NULL, event);
          } else if(field->value->type == EXPRESSION_IDENTIFIER &&
                    strcmp(((IdentifierExpression*)field->value)->name, "parent") == 0) {
            build->interface->takes_parent = true;
          }
        }
      }
    } else if(expression->type == EXPRESSION_REMOVE) {
      add_key(build, ((RemoveExpression*)expression)->key);
    } else if(expression->type == EXPRESSION_SEND) {
      SendExpression* send = (SendExpression*)expression;
      // The data it gives: a :symbol is in the assigns option, as a
      // spawn's is, and parent is the machine's own.
      if(send->data != NULL && send->data->type == EXPRESSION_SYMBOL) {
        bag_add(build, &build->interface->assigns, ((SymbolExpression*)send->data)->name, NULL, event);
      } else if(send->data != NULL && send->data->type == EXPRESSION_IDENTIFIER &&
                strcmp(((IdentifierExpression*)send->data)->name, "parent") == 0) {
        build->interface->takes_parent = true;
      }
      if(send->actor != NULL && send->event != NULL && strcmp(send->actor, "parent") == 0) {
        build->interface->takes_parent = true;
        if(!list_contains(&build->interface->parent_events, send->event)) {
          string_list_append(&build->interface->parent_events, send->event);
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
  string_list_init(&interface->parent_events, arena);
  string_list_init(&interface->keys, arena);
  str_set_init(&interface->key_set, arena);
  machine->interface = interface;
  Build build = { .printer = printer, .machine = machine, .interface = interface };
  collect_states(&build, machine->states);
}

JSSymbolBag* printer_interface_bag(JSInterface* interface, size_t index) {
  JSSymbolBag* bags[INTERFACE_BAGS] = { &interface->actions, &interface->assigns, &interface->delays,
    &interface->guards, &interface->services, &interface->actors };
  return bags[index];
}

const char* printer_interface_option(size_t index) {
  static const char* const options[INTERFACE_BAGS] = { "actions", "assigns", "delays", "guards", "services", "services" };
  return options[index];
}

// Call visit for each spawn (in an assign or an append) of machine's states.
typedef bool (*SpawnVisitor)(Printer*, JSMachine*, AssignExpression*);

static bool visit_spawns_in(Printer* printer, JSMachine* machine, TransitionAction* action, SpawnVisitor visit) {
  bool changed = false;
  for(; action != NULL; action = action->next) {
    AssignExpression* assign = printer_action_spawn(printer, action);
    if(assign != NULL && visit(printer, machine, assign)) {
      changed = true;
    }
  }
  return changed;
}

static bool visit_spawns_of(Printer* printer, JSMachine* machine, TransitionNode* transition, SpawnVisitor visit) {
  bool changed = false;
  for(; transition != NULL; transition = transition->next) {
    if(visit_spawns_in(printer, machine, transition->action, visit)) {
      changed = true;
    }
  }
  return changed;
}

static bool visit_spawns(Printer* printer, JSMachine* machine, JSState* state, SpawnVisitor visit) {
  bool changed = false;
  for(; state != NULL; state = state->next) {
    StateNode* node = state->node;
    if(node->entry != NULL && visit_spawns_in(printer, machine, node->entry->action, visit)) {
      changed = true;
    }
    if(node->exit != NULL && visit_spawns_in(printer, machine, node->exit->action, visit)) {
      changed = true;
    }
    if(visit_spawns_of(printer, machine, node->event_transition, visit) ||
       visit_spawns_of(printer, machine, node->immediate_transition, visit) ||
       visit_spawns_of(printer, machine, node->delay_transition, visit) ||
       (node->invoke != NULL && visit_spawns_of(printer, machine, node->invoke->event_transition, visit)) ||
       visit_spawns(printer, machine, state->child, visit)) {
      changed = true;
    }
  }
  return changed;
}

// The machine of this file that a spawn spawns, or NULL.
static JSMachine* spawned_machine(Printer* printer, SpawnExpression* spawn) {
  if(spawn->target->type != EXPRESSION_IDENTIFIER) {
    return NULL;
  }
  return printer_find_machine(printer, ((IdentifierExpression*)spawn->target)->name);
}

// For each `:name = :symbol` the spawn gives a machine of this file, put
// symbol in the option of machine that the spawned machine has name in.
// Returns whether that added anything.
static bool forward_spawn(Printer* printer, JSMachine* machine, AssignExpression* assign) {
  SpawnExpression* spawn = (SpawnExpression*)assign->value;
  JSMachine* child = spawned_machine(printer, spawn);
  if(child == NULL) {
    return false;
  }
  bool changed = false;
  for(SpawnField* field = spawn->fields; field != NULL; field = field->next) {
    if(!field->symbol || field->value->type != EXPRESSION_SYMBOL) {
      continue;
    }
    const char* name = ((SymbolExpression*)field->value)->name;
    for(size_t i = 0; i < INTERFACE_BAGS; i++) {
      JSSymbolBag* bag = printer_interface_bag(machine->interface, i);
      if(str_map_get(printer_interface_bag(child->interface, i)->by_name, field->name) == NULL ||
         str_map_get(bag->by_name, name) != NULL) {
        continue;
      }
      JSSymbol* symbol = ARENA_NEW(printer->arena, JSSymbol);
      symbol->name = name;
      symbol->forwarded = true;
      string_list_init(&symbol->events, printer->arena);
      str_map_set(bag->by_name, name, symbol);
      *bag->tail = symbol;
      bag->tail = &symbol->next;
      changed = true;
    }
  }
  return changed;
}

void printer_forward_symbols(Printer* printer) {
  // A machine may be given a symbol only to give it to a machine it spawns
  // in turn, so go round until nothing is added.
  bool changed = true;
  while(changed) {
    changed = false;
    for(JSMachine* machine = printer->machines; machine != NULL; machine = machine->next) {
      if(visit_spawns(printer, machine, machine->states, forward_spawn)) {
        changed = true;
      }
    }
  }
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

// Warn, once for each machine that machine creates, about what child
// expects of its creator that it doesn't get: a parent that declares the
// events it sends and, of an invoke, the symbols it uses. An invoke can't
// give any; a spawn can, which check_symbols looks at.
static void check_child(Printer* printer, JSMachine* machine, JSMachine* child, const char* how, bool gives_symbols, StrSet* warned) {
  JSInterface* needs = child->interface;
  if(!str_set_add(warned, child->name)) {
    return;
  }
  // The events the child sends its parent, which this machine is, are
  // ones it should declare, if it declares its events.
  if(machine->events != NULL) {
    for(StringListIterator it = string_list_iterator(&needs->parent_events); string_list_next(&it);) {
      if(printer_find_event(machine, it.node->value) == NULL) {
        printer_warnf(printer, LUCY_STREAM_STDOUT,
          "Machine [%s] sends its parent the event [%s], which machine [%s], which %s it, does not declare.\n",
          child->name, it.node->value, machine->name, how);
      }
    }
  }
  if(gives_symbols || !printer_interface_needs_options(needs)) {
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

// Warn about the inputs a spawn gives a machine that declares its own:
// the ones it leaves out, and the ones the machine doesn't have. A machine
// that declares none isn't checked.
static void check_inputs(Printer* printer, JSState* state, AssignExpression* assign, JSMachine* child) {
  if(child->inputs == NULL) {
    return;
  }
  SpawnExpression* spawn = (SpawnExpression*)assign->value;
  for(JSInput* input = child->inputs; input != NULL; input = input->next) {
    bool given = false;
    for(SpawnField* field = spawn->fields; field != NULL; field = field->next) {
      if(!field->symbol && strcmp(field->name, input->name) == 0) {
        given = true;
      }
    }
    if(!given) {
      printer_warnf(printer, LUCY_STREAM_STDOUT,
        "spawn(%s) for [%s] in state [%s] doesn't give machine [%s] its input [%s].\n",
        child->name, assign->key, state->path, child->name, input->name);
    }
  }
  for(SpawnField* field = spawn->fields; field != NULL; field = field->next) {
    if(!field->symbol && printer_find_input(child, field->name) == NULL) {
      printer_warnf(printer, LUCY_STREAM_STDOUT,
        "spawn(%s) for [%s] in state [%s] gives [%s], which machine [%s] doesn't declare as an input.\n",
        child->name, assign->key, state->path, field->name, child->name);
    }
  }
}

static bool gives_symbol(const SpawnExpression* spawn, const char* name) {
  for(const SpawnField* field = spawn->fields; field != NULL; field = field->next) {
    if(field->symbol && strcmp(field->name, name) == 0) {
      return true;
    }
  }
  return false;
}

// Warn about the symbols a spawn gives a machine of this file: the ones
// the machine uses that it leaves out, and the ones the machine doesn't
// use.
static void check_symbols(Printer* printer, JSState* state, AssignExpression* assign, JSMachine* child) {
  SpawnExpression* spawn = (SpawnExpression*)assign->value;
  StrBuilder* missing = str_builder_create();
  for(size_t i = 0; i < INTERFACE_BAGS; i++) {
    for(JSSymbol* symbol = printer_interface_bag(child->interface, i)->head; symbol != NULL; symbol = symbol->next) {
      if(!gives_symbol(spawn, symbol->name)) {
        if(str_builder_peek(missing)[0] != '\0') {
          str_builder_add_str(missing, ", ");
        }
        str_builder_add_str(missing, printer_interface_option(i));
        str_builder_add_str(missing, ".");
        str_builder_add_str(missing, symbol->name);
      }
    }
  }
  if(str_builder_peek(missing)[0] != '\0') {
    printer_warnf(printer, LUCY_STREAM_STDOUT,
      "spawn(%s) for [%s] in state [%s] doesn't give machine [%s] the symbols it uses (%s), so it will throw when it uses them.\n",
      child->name, assign->key, state->path, child->name, str_builder_peek(missing));
  }
  str_builder_destroy(missing);
  for(SpawnField* field = spawn->fields; field != NULL; field = field->next) {
    if(!field->symbol) {
      continue;
    }
    bool used = false;
    for(size_t i = 0; i < INTERFACE_BAGS; i++) {
      if(str_map_get(printer_interface_bag(child->interface, i)->by_name, field->name) != NULL) {
        used = true;
      }
    }
    if(!used) {
      printer_warnf(printer, LUCY_STREAM_STDOUT,
        "spawn(%s) for [%s] in state [%s] gives [:%s], which machine [%s] doesn't use.\n",
        child->name, assign->key, state->path, field->name, child->name);
    }
  }
}

static void check_spawns(Printer* printer, JSMachine* machine, JSState* state, TransitionAction* action, StrSet* warned) {
  for(; action != NULL; action = action->next) {
    AssignExpression* assign = printer_action_spawn(printer, action);
    if(assign == NULL) {
      continue;
    }
    SpawnExpression* spawn = (SpawnExpression*)assign->value;
    JSMachine* child = spawned_machine(printer, spawn);
    if(child != NULL) {
      check_child(printer, machine, child, "spawns", true, warned);
      check_inputs(printer, state, assign, child);
      check_symbols(printer, state, assign, child);
      continue;
    }
    // What anything else uses its symbols for isn't known.
    for(SpawnField* field = spawn->fields; field != NULL; field = field->next) {
      if(field->symbol) {
        printer_warnf(printer, LUCY_STREAM_STDOUT,
          "The spawn for [%s] in state [%s] gives [:%s], but a symbol can only be given to a machine of this file.\n",
          assign->key, state->path, field->name);
      }
    }
  }
}

static void check_transitions(Printer* printer, JSMachine* machine, JSState* state, TransitionNode* transition, StrSet* warned) {
  for(; transition != NULL; transition = transition->next) {
    check_spawns(printer, machine, state, transition->action, warned);
  }
}

static void check_states(Printer* printer, JSMachine* machine, JSState* state, StrSet* warned) {
  for(; state != NULL; state = state->next) {
    StateNode* node = state->node;
    if(node->entry != NULL) {
      check_spawns(printer, machine, state, node->entry->action, warned);
    }
    if(node->exit != NULL) {
      check_spawns(printer, machine, state, node->exit->action, warned);
    }
    check_transitions(printer, machine, state, node->event_transition, warned);
    check_transitions(printer, machine, state, node->immediate_transition, warned);
    check_transitions(printer, machine, state, node->delay_transition, warned);
    if(node->invoke != NULL) {
      check_transitions(printer, machine, state, node->invoke->event_transition, warned);
    }
    if(state->invoke_kind == INVOKE_MACHINE) {
      check_child(printer, machine, state->invoked, "invokes", false, warned);
      // An invoke has nowhere to give inputs.
      for(JSInput* input = state->invoked->inputs; input != NULL; input = input->next) {
        printer_warnf(printer, LUCY_STREAM_STDOUT,
          "invoke(%s) in state [%s] doesn't give machine [%s] its input [%s]: an invoke can't give any.\n",
          state->invoked->name, state->path, state->invoked->name, input->name);
      }
    }
    check_states(printer, machine, state->child, warned);
  }
}

void printer_check_interfaces(Printer* printer, JSMachine* machine) {
  StrSet warned;
  str_set_init(&warned, printer->arena);
  check_states(printer, machine, machine->states, &warned);
}
