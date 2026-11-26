#include "printer.h"

#include <stdbool.h>
#include <string.h>

Expression* printer_action_expression(Printer* printer, TransitionAction* action) {
  if(action->name == NULL) {
    return action->expression;
  }
  AssignmentNode* binding = str_map_get(printer->bindings, action->name);
  if(binding == NULL) {
    printer_warnf(printer, LUCY_STREAM_STDOUT, "Unknown action [%s].\n", action->name);
    return NULL;
  }
  return binding->value;
}

void printer_add_child_options(Printer* printer, JSMachine* child) {
  if(child->interface->takes_parent) {
    js_builder_add_str(printer->jsb, "{ parent: machine }");
  }
}

// The function spawn(x) calls: a machine's factory, an imported function
// or a service. *machine_out gets the machine, when it is one of this
// file's.
static void add_spawn_callee(Printer* printer, SpawnExpression* spawn, JSMachine** machine_out) {
  JSBuilder* jsb = printer->jsb;
  Expression* target = spawn->target;
  *machine_out = NULL;
  if(target->type == EXPRESSION_IDENTIFIER) {
    const char* name = ((IdentifierExpression*)target)->name;
    *machine_out = printer_find_machine(printer, name);
    js_builder_add_str(jsb, *machine_out != NULL ? (*machine_out)->factory : name);
  } else if(target->type == EXPRESSION_SYMBOL) {
    js_builder_add_format(jsb, "services.%s", ((SymbolExpression*)target)->name);
  }
}

// A value a spawn gives, which is a reference.
static void add_value(Printer* printer, Expression* value) {
  JSBuilder* jsb = printer->jsb;
  if(value->type == EXPRESSION_MEMBER) {
    MemberExpression* member = (MemberExpression*)value;
    const char* property = ((IdentifierExpression*)member->property)->name;
    if(strcmp(member->owner, "context") == 0) {
      js_builder_add_format(jsb, "machine.context.%s", property);
    } else {
      js_builder_add_format(jsb, "event.%s", property);
    }
  } else if(value->type == EXPRESSION_IDENTIFIER) {
    const char* name = ((IdentifierExpression*)value)->name;
    if(strcmp(name, "self") == 0) {
      js_builder_add_str(jsb, "machine");
    } else if(strcmp(name, "parent") == 0) {
      js_builder_add_str(jsb, printer->parent);
    } else {
      js_builder_add_format(jsb, "%s(machine.context, event)", name);
    }
  } else if(value->type == EXPRESSION_SYMBOL) {
    js_builder_add_format(jsb, "assigns.%s(machine.context, event)", ((SymbolExpression*)value)->name);
  }
}

// Whether a value uses the event: all but context.key, self and parent.
static bool value_takes_event(Expression* value) {
  if(value->type == EXPRESSION_MEMBER) {
    return strcmp(((MemberExpression*)value)->owner, "event") == 0;
  }
  if(value->type == EXPRESSION_IDENTIFIER) {
    const char* name = ((IdentifierExpression*)value)->name;
    return strcmp(name, "self") != 0 && strcmp(name, "parent") != 0;
  }
  return true;
}

bool printer_spawn_takes_event(SpawnExpression* spawn) {
  for(SpawnField* field = spawn->fields; field != NULL; field = field->next) {
    // A symbol's function is handed over, not called.
    if(!field->symbol && value_takes_event(field->value)) {
      return true;
    }
  }
  return false;
}

static bool gives_inputs(const SpawnExpression* spawn) {
  for(const SpawnField* field = spawn->fields; field != NULL; field = field->next) {
    if(!field->symbol) {
      return true;
    }
  }
  return false;
}

// Whether machine, which a spawn spawns, has the symbol a field gives in
// the option with this name. The services option holds two of an
// interface's bags.
static bool option_has(JSMachine* machine, const char* option, const SpawnField* field) {
  if(machine == NULL || !field->symbol) {
    return false;
  }
  for(size_t i = 0; i < INTERFACE_BAGS; i++) {
    if(strcmp(printer_interface_option(i), option) == 0 &&
       str_map_get(printer_interface_bag(machine->interface, i)->by_name, field->name) != NULL) {
      return true;
    }
  }
  return false;
}

static bool gives_option(JSMachine* machine, const SpawnExpression* spawn, const char* option) {
  for(const SpawnField* field = spawn->fields; field != NULL; field = field->next) {
    if(option_has(machine, option, field)) {
      return true;
    }
  }
  return false;
}

// The options a machine can be given symbols in, in the order printed.
static const char* const spawn_options[] = { "actions", "assigns", "delays", "guards", "services", NULL };

static JSMachine* spawned_machine(Printer* printer, SpawnExpression* spawn) {
  if(spawn->target->type != EXPRESSION_IDENTIFIER) {
    return NULL;
  }
  return printer_find_machine(printer, ((IdentifierExpression*)spawn->target)->name);
}

bool printer_spawn_is_long(Printer* printer, SpawnExpression* spawn) {
  if(gives_inputs(spawn)) {
    return true;
  }
  JSMachine* machine = spawned_machine(printer, spawn);
  for(size_t i = 0; spawn_options[i] != NULL; i++) {
    if(gives_option(machine, spawn, spawn_options[i])) {
      return true;
    }
  }
  return false;
}

// What spawn(x) is in the output, from where the line is up to: the call
// that creates what x makes and starts it. A machine of this file is given
// its parent if it uses it. What anything else does with it isn't known,
// so it always is.
//
// With a block it is over several lines: the inputs, which are the context
// the machine starts with, and the symbols, each in the option that the
// machine has it in (which only a machine of this file is known to have).
void printer_add_spawn_call(Printer* printer, SpawnExpression* spawn, bool start) {
  JSBuilder* jsb = printer->jsb;
  JSMachine* machine = NULL;
  add_spawn_callee(printer, spawn, &machine);
  bool with_parent = machine == NULL || machine->interface->takes_parent;

  if(!printer_spawn_is_long(printer, spawn)) {
    js_builder_add_str(jsb, with_parent ? "({ parent: machine })" : "()");
    js_builder_add_str(jsb, start ? ".start()" : "");
    return;
  }

  // How many parts follow the one being printed, for the commas.
  size_t parts = 0;
  if(gives_inputs(spawn)) {
    parts++;
  }
  if(with_parent) {
    parts++;
  }
  for(size_t i = 0; spawn_options[i] != NULL; i++) {
    if(gives_option(machine, spawn, spawn_options[i])) {
      parts++;
    }
  }

  js_builder_add_str(jsb, "({\n");
  jsb->depth++;
  if(gives_inputs(spawn)) {
    js_builder_open(jsb, "context: {");
    const SpawnField* last = NULL;
    for(const SpawnField* field = spawn->fields; field != NULL; field = field->next) {
      if(!field->symbol) {
        last = field;
      }
    }
    for(SpawnField* field = spawn->fields; field != NULL; field = field->next) {
      if(field->symbol) {
        continue;
      }
      js_builder_add_indent(jsb);
      js_builder_add_format(jsb, "%s: ", field->name);
      add_value(printer, field->value);
      js_builder_add_str(jsb, field != last ? ",\n" : "\n");
    }
    js_builder_close(jsb, "}%s", --parts > 0 ? "," : "");
  }
  for(size_t i = 0; spawn_options[i] != NULL; i++) {
    const char* option = spawn_options[i];
    if(!gives_option(machine, spawn, option)) {
      continue;
    }
    js_builder_open(jsb, "%s: {", option);
    const SpawnField* last = NULL;
    for(const SpawnField* field = spawn->fields; field != NULL; field = field->next) {
      if(option_has(machine, option, field)) {
        last = field;
      }
    }
    for(const SpawnField* field = spawn->fields; field != NULL; field = field->next) {
      if(!option_has(machine, option, field)) {
        continue;
      }
      const char* comma = field != last ? "," : "";
      if(field->value->type == EXPRESSION_SYMBOL) {
        // One of this machine's own, which it has in the same option.
        js_builder_line(jsb, "%s: %s.%s%s", field->name, option, ((SymbolExpression*)field->value)->name, comma);
      } else {
        const char* function = ((IdentifierExpression*)field->value)->name;
        if(strcmp(function, field->name) == 0) {
          js_builder_line(jsb, "%s%s", function, comma);
        } else {
          js_builder_line(jsb, "%s: %s%s", field->name, function, comma);
        }
      }
    }
    js_builder_close(jsb, "}%s", --parts > 0 ? "," : "");
  }
  if(with_parent) {
    js_builder_line(jsb, "parent: machine");
  }
  jsb->depth--;
  js_builder_add_indent(jsb);
  js_builder_add_str(jsb, start ? "}).start()" : "})");
}

// assign(key, spawn(x)): the machine is kept in the context, in place of
// what the key held. append(key, spawn(x)): it is added to the list the
// key holds.
static void print_spawn(Printer* printer, AssignExpression* assign, SpawnExpression* spawn) {
  JSBuilder* jsb = printer->jsb;
  if(assign->append) {
    js_builder_open(jsb, "machine.context = {");
    js_builder_line(jsb, "...machine.context,");
    js_builder_open(jsb, "%s: [", assign->key);
    // There need be no list yet.
    js_builder_line(jsb, "...(machine.context.%s ?? []),", assign->key);
    js_builder_add_indent(jsb);
    printer_add_spawn_call(printer, spawn, true);
    js_builder_add_str(jsb, "\n");
    js_builder_close(jsb, "]");
    js_builder_close(jsb, "};");
    return;
  }

  // The key may hold what an earlier spawn put there.
  js_builder_line(jsb, "machine.context.%s?.stop();", assign->key);
  if(!printer_spawn_is_long(printer, spawn)) {
    js_builder_add_indent(jsb);
    js_builder_add_format(jsb, "machine.context = { ...machine.context, %s: ", assign->key);
    printer_add_spawn_call(printer, spawn, true);
    js_builder_add_str(jsb, " };\n");
    return;
  }
  js_builder_open(jsb, "machine.context = {");
  js_builder_line(jsb, "...machine.context,");
  js_builder_add_indent(jsb);
  js_builder_add_format(jsb, "%s: ", assign->key);
  printer_add_spawn_call(printer, spawn, true);
  js_builder_add_str(jsb, "\n");
  js_builder_close(jsb, "};");
}

// remove(key, value): the machine is stopped and taken out of the list,
// if it is in it.
static void print_remove(Printer* printer, RemoveExpression* remove) {
  JSBuilder* jsb = printer->jsb;
  js_builder_add_indent(jsb);
  js_builder_add_format(jsb, "if(machine.context.%s?.includes(", remove->key);
  add_value(printer, remove->value);
  js_builder_add_str(jsb, ")) {\n");
  jsb->depth++;
  js_builder_add_indent(jsb);
  add_value(printer, remove->value);
  js_builder_add_str(jsb, ".stop();\n");
  js_builder_add_indent(jsb);
  js_builder_add_format(jsb, "machine.context = { ...machine.context, %s: machine.context.%s.filter(%s => %s !== ",
    remove->key, remove->key, printer->actor, printer->actor);
  add_value(printer, remove->value);
  js_builder_add_str(jsb, ") };\n");
  if(printer_view_has_list(printer, remove->key)) {
    // One machine was taken out, so its view goes.
    js_builder_add_indent(jsb);
    js_builder_add_str(jsb, "for(const node of ");
    add_value(printer, remove->value);
    js_builder_add_format(jsb, ".%s) {\n", printer->nodes);
    jsb->depth++;
    js_builder_line(jsb, "node.remove();");
    js_builder_close(jsb, "}");
  }
  js_builder_close(jsb, "}");
}

static void print_assign(Printer* printer, AssignExpression* assign) {
  JSBuilder* jsb = printer->jsb;
  Expression* value = assign->value;
  if(value != NULL && value->type == EXPRESSION_SPAWN) {
    print_spawn(printer, assign, (SpawnExpression*)value);
    printer_print_view_assign(printer, assign->key, assign->append);
    return;
  }
  js_builder_add_indent(jsb);
  js_builder_add_format(jsb, "machine.context = { ...machine.context, %s: ", assign->key);
  if(value == NULL) {
    js_builder_add_str(jsb, "event.data");
  } else {
    switch(value->type) {
      case EXPRESSION_IDENTIFIER: {
        js_builder_add_format(jsb, "%s(machine.context, event)", ((IdentifierExpression*)value)->name);
        break;
      }
      case EXPRESSION_SYMBOL: {
        js_builder_add_format(jsb, "assigns.%s(machine.context, event)", ((SymbolExpression*)value)->name);
        break;
      }
      default: {
        js_builder_add_str(jsb, "undefined");
        break;
      }
    }
  }
  js_builder_add_str(jsb, " };\n");
  // A view that shows the key shows the change, before anything after
  // the assign in the transition.
  printer_print_view_assign(printer, assign->key, false);
}

static void print_send(Printer* printer, SendExpression* send) {
  JSBuilder* jsb = printer->jsb;
  js_builder_add_indent(jsb);
  if(send->actor != NULL && strcmp(send->actor, "parent") == 0) {
    // A machine nothing created has no parent to tell.
    js_builder_add_format(jsb, "%s?.send({ type: '%s', data: ", printer->parent, send->event);
  } else {
    js_builder_add_format(jsb, "machine.context.%s.send({ type: '%s', data: ", send->actor, send->event);
  }
  // The data it names, or else that of the event being handled.
  if(send->data != NULL) {
    add_value(printer, send->data);
  } else {
    js_builder_add_str(jsb, "event.data");
  }
  js_builder_add_str(jsb, " });\n");
}

// A call of a function given as an identifier (imported) or a symbol
// (from the options bag named by bag).
static void print_call(Printer* printer, Expression* ref, const char* bag) {
  if(ref->type == EXPRESSION_IDENTIFIER) {
    js_builder_line(printer->jsb, "%s(machine.context, event);", ((IdentifierExpression*)ref)->name);
  } else if(ref->type == EXPRESSION_SYMBOL) {
    js_builder_line(printer->jsb, "%s.%s(machine.context, event);", bag, ((SymbolExpression*)ref)->name);
  }
}

void printer_print_action(Printer* printer, TransitionAction* action) {
  Expression* expression = printer_action_expression(printer, action);
  if(expression == NULL) {
    return;
  }
  switch(expression->type) {
    case EXPRESSION_ASSIGN: {
      print_assign(printer, (AssignExpression*)expression);
      break;
    }
    case EXPRESSION_SEND: {
      print_send(printer, (SendExpression*)expression);
      break;
    }
    case EXPRESSION_REMOVE: {
      print_remove(printer, (RemoveExpression*)expression);
      break;
    }
    case EXPRESSION_ACTION: {
      print_call(printer, ((ActionExpression*)expression)->ref, "actions");
      break;
    }
    case EXPRESSION_IDENTIFIER:
    case EXPRESSION_SYMBOL: {
      print_call(printer, expression, "actions");
      break;
    }
    default: {
      printer_warn(printer, LUCY_STREAM_STDOUT, "This type of expression is not currently supported.\n");
      break;
    }
  }
}

bool printer_action_takes_event(Printer* printer, TransitionAction* action) {
  Expression* expression = printer_action_expression(printer, action);
  if(expression == NULL) {
    return false;
  }
  // Everything is called with the event, except a spawn, which only has a
  // use for it in the values it gives.
  if(expression->type == EXPRESSION_ASSIGN) {
    AssignExpression* assign = (AssignExpression*)expression;
    if(assign->value != NULL && assign->value->type == EXPRESSION_SPAWN) {
      return printer_spawn_takes_event((SpawnExpression*)assign->value);
    }
    return true;
  }
  if(expression->type == EXPRESSION_SEND && ((SendExpression*)expression)->data != NULL) {
    return value_takes_event(((SendExpression*)expression)->data);
  }
  if(expression->type == EXPRESSION_REMOVE) {
    return value_takes_event(((RemoveExpression*)expression)->value);
  }
  return true;
}

bool printer_actions_take_event(Printer* printer, TransitionAction* action) {
  for(; action != NULL; action = action->next) {
    if(printer_action_takes_event(printer, action)) {
      return true;
    }
  }
  return false;
}

AssignExpression* printer_action_spawn(Printer* printer, TransitionAction* action) {
  Expression* expression = printer_action_expression(printer, action);
  if(expression == NULL || expression->type != EXPRESSION_ASSIGN) {
    return NULL;
  }
  AssignExpression* assign = (AssignExpression*)expression;
  if(assign->value == NULL || assign->value->type != EXPRESSION_SPAWN) {
    return NULL;
  }
  return assign;
}

void printer_add_delay_length(Printer* printer, DelayExpression* delay) {
  JSBuilder* jsb = printer->jsb;
  if(delay->ref == NULL) {
    js_builder_add_format(jsb, "%d", delay->time);
  } else if(delay->ref->type == EXPRESSION_IDENTIFIER) {
    js_builder_add_format(jsb, "%s(machine.context, event)", ((IdentifierExpression*)delay->ref)->name);
  } else if(delay->ref->type == EXPRESSION_SYMBOL) {
    js_builder_add_format(jsb, "delays.%s(machine.context, event)", ((SymbolExpression*)delay->ref)->name);
  }
}

void printer_add_guards(Printer* printer, TransitionGuard* guard) {
  JSBuilder* jsb = printer->jsb;
  for(; guard != NULL; guard = guard->next) {
    Expression* ref = NULL;
    if(guard->name != NULL) {
      AssignmentNode* binding = str_map_get(printer->bindings, guard->name);
      if(binding == NULL) {
        printer_warnf(printer, LUCY_STREAM_STDOUT, "Unknown guard [%s].\n", guard->name);
      } else {
        ref = binding->value;
      }
    } else if(guard->expression != NULL) {
      ref = guard->expression->ref;
    }
    if(ref == NULL) {
      js_builder_add_str(jsb, "true");
    } else if(ref->type == EXPRESSION_IDENTIFIER) {
      js_builder_add_format(jsb, "%s(machine.context, event)", ((IdentifierExpression*)ref)->name);
    } else if(ref->type == EXPRESSION_SYMBOL) {
      js_builder_add_format(jsb, "guards.%s(machine.context, event)", ((SymbolExpression*)ref)->name);
    } else {
      js_builder_add_str(jsb, "true");
    }
    if(guard->next != NULL) {
      js_builder_add_str(jsb, " && ");
    }
  }
}
