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
    if(value_takes_event(field->value)) {
      return true;
    }
  }
  return false;
}

// What spawn(x) is in the output, from where the line is up to: the call
// that creates what x makes and starts it. With inputs, which are the
// context it starts with, it is over several lines. A machine of this file
// is given its parent if it uses it. What anything else does with it isn't
// known, so it always is.
static void add_spawn_call(Printer* printer, SpawnExpression* spawn) {
  JSBuilder* jsb = printer->jsb;
  JSMachine* machine = NULL;
  add_spawn_callee(printer, spawn, &machine);

  if(spawn->fields == NULL) {
    js_builder_add_str(jsb, "(");
    if(machine != NULL) {
      printer_add_child_options(printer, machine);
    } else {
      js_builder_add_str(jsb, "{ parent: machine }");
    }
    js_builder_add_str(jsb, ").start()");
    return;
  }

  js_builder_add_str(jsb, "({\n");
  jsb->depth++;
  bool with_parent = machine == NULL || machine->interface->takes_parent;
  js_builder_open(jsb, "context: {");
  for(SpawnField* field = spawn->fields; field != NULL; field = field->next) {
    js_builder_add_indent(jsb);
    js_builder_add_format(jsb, "%s: ", field->name);
    add_value(printer, field->value);
    js_builder_add_str(jsb, field->next != NULL ? ",\n" : "\n");
  }
  js_builder_close(jsb, "}%s", with_parent ? "," : "");
  if(with_parent) {
    js_builder_line(jsb, "parent: machine");
  }
  jsb->depth--;
  js_builder_add_indent(jsb);
  js_builder_add_str(jsb, "}).start()");
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
    add_spawn_call(printer, spawn);
    js_builder_add_str(jsb, "\n");
    js_builder_close(jsb, "]");
    js_builder_close(jsb, "};");
    return;
  }

  // The key may hold what an earlier spawn put there.
  js_builder_line(jsb, "machine.context.%s?.stop();", assign->key);
  if(spawn->fields == NULL) {
    js_builder_add_indent(jsb);
    js_builder_add_format(jsb, "machine.context = { ...machine.context, %s: ", assign->key);
    add_spawn_call(printer, spawn);
    js_builder_add_str(jsb, " };\n");
    return;
  }
  js_builder_open(jsb, "machine.context = {");
  js_builder_line(jsb, "...machine.context,");
  js_builder_add_indent(jsb);
  js_builder_add_format(jsb, "%s: ", assign->key);
  add_spawn_call(printer, spawn);
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
  js_builder_close(jsb, "}");
}

static void print_assign(Printer* printer, AssignExpression* assign) {
  JSBuilder* jsb = printer->jsb;
  Expression* value = assign->value;
  if(value != NULL && value->type == EXPRESSION_SPAWN) {
    print_spawn(printer, assign, (SpawnExpression*)value);
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
