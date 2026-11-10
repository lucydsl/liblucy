#include "printer.h"

#include <stdbool.h>

// The expression an action stands for: the value of its `action name =`
// binding when it is a named action, else its own expression. NULL, after
// a warning, for a name with no binding.
static Expression* action_expression(Printer* printer, TransitionAction* action) {
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

void printer_add_spawn(Printer* printer, SpawnExpression* spawn) {
  JSBuilder* jsb = printer->jsb;
  Expression* target = spawn->target;
  if(target->type == EXPRESSION_IDENTIFIER) {
    const char* name = ((IdentifierExpression*)target)->name;
    JSMachine* machine = printer_find_machine(printer, name);
    js_builder_add_str(jsb, machine != NULL ? machine->factory : name);
  } else if(target->type == EXPRESSION_SYMBOL) {
    js_builder_add_format(jsb, "services.%s", ((SymbolExpression*)target)->name);
  }
  js_builder_add_str(jsb, "().start()");
}

static void print_assign(Printer* printer, AssignExpression* assign) {
  JSBuilder* jsb = printer->jsb;
  js_builder_add_indent(jsb);
  js_builder_add_format(jsb, "machine.context = { ...machine.context, %s: ", assign->key);
  Expression* value = assign->value;
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
      case EXPRESSION_SPAWN: {
        printer_add_spawn(printer, (SpawnExpression*)value);
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
  js_builder_line(printer->jsb, "machine.context.%s.send({ type: '%s', data: event.data });",
    send->actor, send->event);
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
  Expression* expression = action_expression(printer, action);
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
  Expression* expression = action_expression(printer, action);
  if(expression == NULL) {
    return false;
  }
  // Everything is called with the event, except a spawn, which has no use
  // for it.
  if(expression->type == EXPRESSION_ASSIGN) {
    AssignExpression* assign = (AssignExpression*)expression;
    return assign->value == NULL || assign->value->type != EXPRESSION_SPAWN;
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
  Expression* expression = action_expression(printer, action);
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
