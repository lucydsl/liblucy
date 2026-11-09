#include "printer.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

static void add_machine_fn_args(Printer* printer, JSBuilder* jsb, MachineNode* machine_node) {
  js_builder_add_str(jsb, "{ ");
  if(machine_node->flags & MACHINE_USES_ACTION) {
    js_builder_add_arg(jsb, "actions");
  }
  if(machine_node->flags & MACHINE_USES_ASSIGN) {
    js_builder_add_arg(jsb, "assigns");
  }
  js_builder_add_arg(jsb, "context");
  js_builder_add_str(jsb, " = {}");
  if(machine_node->flags & MACHINE_USES_DELAY) {
    js_builder_add_arg(jsb, "delays");
  }
  if(machine_node->flags & MACHINE_USES_GUARD) {
    js_builder_add_arg(jsb, "guards");
  }
  if(machine_node->flags & MACHINE_USES_SERVICE) {
    js_builder_add_arg(jsb, "services");
  }
  js_builder_add_str(jsb, " } = {}");
}

void printer_add_machine_binding_name(Printer* printer, JSBuilder* jsb, MachineNode* machine_node) {
  js_builder_add_str(jsb, "create");
  js_builder_add_char(jsb, toupper((unsigned char)printer->source[machine_node->name_start]));
  size_t i = machine_node->name_start + 1;
  while(i < machine_node->name_end) {
    js_builder_add_char(jsb, printer->source[i]);
    i++;
  }
}

static inline void compile_machine(Printer* printer, JSBuilder* jsb, MachineNode* machine_node, bool is_nested) {
  if(!is_nested) {
    if(machine_node->name_start == 0) {
      js_builder_add_str(jsb, "\nexport default function(");
      add_machine_fn_args(printer, jsb, machine_node);
      js_builder_add_str(jsb, ") {\n");
      js_builder_increase_indent(jsb);
    } else {
      js_builder_add_export(jsb);
      js_builder_add_str(jsb, "function ");
      printer_add_machine_binding_name(printer, jsb, machine_node);
      js_builder_add_str(jsb, "(");
      add_machine_fn_args(printer, jsb, machine_node);
      js_builder_add_str(jsb, ") {\n");
      js_builder_increase_indent(jsb);
    }
    js_builder_start_call(jsb, "return createMachine");
    js_builder_start_object(jsb);
  }

  if(machine_node->initial_start != 0) {
    js_builder_start_prop(jsb, "initial");
    js_builder_copy_string(jsb, printer->source, machine_node->initial_start, machine_node->initial_end);
  }
  js_builder_shorthand_prop(jsb, "context");
}

void printer_enter_machine(Printer* printer, JSBuilder* jsb, Node* node) {
  MachineNode *machine_node = (MachineNode*)node;
  Node* parent_node = node->parent;
  bool is_nested = node_is_nested_machine(node);
  compile_machine(printer, jsb, machine_node, is_nested);
  if(printer->ts_printer != NULL) {
    ts_printer_add_machine_name(printer->ts_printer, machine_node->name_start, machine_node->name_end);
  }
}

void printer_exit_machine(Printer* printer, JSBuilder* jsb, Node* node) {
  bool has_guard = printer->guard_refs != NULL;
  bool has_action = printer->action_refs != NULL;
  bool has_delay = printer->delay_refs != NULL;
  bool has_service = printer->service_refs != NULL;
  bool needs_options = has_guard || has_action || has_delay || has_service;
  bool is_nested = node_is_nested_machine(node);

  if(!is_nested && needs_options) {
    js_builder_end_object(jsb);
    js_builder_add_str(jsb, ", ");

    js_builder_start_object(jsb);

    if(has_guard) {
      js_builder_start_prop(jsb, "guards");
      js_builder_start_object(jsb);

      Ref* ref = printer->guard_refs;
      while(ref != NULL) {
        js_builder_start_prop(jsb, ref->key);

        Expression* expression = ref->value;

        switch(expression->type) {
          case EXPRESSION_IDENTIFIER: {
            char* identifier = ((IdentifierExpression*)expression)->name;
            js_builder_add_str(jsb, identifier);
            break;
          }
          case EXPRESSION_SYMBOL: {
            char* identifier = ((SymbolExpression*)expression)->name;
            js_builder_add_str(jsb, "guards.");
            js_builder_add_str(jsb, identifier);
            break;
          }
          default: {
            printer_warn(printer, LUCY_STREAM_STDOUT, "Unexpected type of expression\n");
            goto end_guardloop;
          }
        }

        ref = ref->next;
      }

      end_guardloop: {};

      js_builder_end_object(jsb);
    }

    if(has_action) {
      js_builder_start_prop(jsb, "actions");
      js_builder_start_object(jsb);

      Ref* ref = printer->action_refs;
      while(ref != NULL) {
        js_builder_start_prop(jsb, ref->key);

        Expression* expression = ref->value;

        switch(expression->type) {
          case EXPRESSION_ASSIGN: {
            AssignExpression* assign = (AssignExpression*)expression;

            printer_start_assign_call(jsb, assign);

            // assign(key) has no value. It used to crash here; print what
            // an inline assign(key) prints instead.
            if(assign->value == NULL) {
              js_builder_add_str(jsb, "(context, event) => event.data");
            } else switch(assign->value->type) {
              case EXPRESSION_IDENTIFIER: {
                char* identifier = ((IdentifierExpression*)assign->value)->name;
                js_builder_add_str(jsb, identifier);
                break;
              }
              case EXPRESSION_SPAWN: {
                SpawnExpression* spawn_expression = (SpawnExpression*)assign->value;
                printer_add_spawn_call(printer, jsb, spawn_expression);
                break;
              }
              case EXPRESSION_SYMBOL: {
                SymbolExpression* symbol_expression = (SymbolExpression*)assign->value;
                js_builder_add_str(jsb, "assigns.");
                js_builder_add_str(jsb, symbol_expression->name);
                break;
              }
              default: {
                break;
              }
            }

            printer_end_assign_call(jsb);
            break;
          }
          case EXPRESSION_SEND: {
            SendExpression* send_expression = (SendExpression*)expression;
            printer_add_send_call(jsb, send_expression);
            break;
          }
          case EXPRESSION_IDENTIFIER: {
            IdentifierExpression* identifier_expression = (IdentifierExpression*)expression;
            js_builder_add_str(jsb, identifier_expression->name);
            break;
          }
          case EXPRESSION_SYMBOL: {
            SymbolExpression* symbol_expression = (SymbolExpression*)expression;
            js_builder_add_str(jsb, "actions.");
            js_builder_add_str(jsb, symbol_expression->name);
            break;
          }
          default: {
            printer_warn(printer, LUCY_STREAM_STDOUT, "This type of expression is not currently supported.\n");
            break;
          }
        }

        ref = ref->next;
      }

      js_builder_end_object(jsb);
    }

    if(has_delay) {
      js_builder_start_prop(jsb, "delays");
      js_builder_start_object(jsb);

      Ref* ref = printer->delay_refs;
      while(ref != NULL) {
        DelayExpression* expression = (DelayExpression*)ref->value;
        if(expression->ref->type == EXPRESSION_IDENTIFIER) {
          IdentifierExpression* identifier_expression = (IdentifierExpression*)expression->ref;
          js_builder_start_prop(jsb, identifier_expression->name);
          js_builder_add_str(jsb, identifier_expression->name);
        } else if(expression->ref->type == EXPRESSION_SYMBOL) {
          SymbolExpression* symbol_expression = (SymbolExpression*)expression->ref;
          js_builder_start_prop(jsb, symbol_expression->name);
          js_builder_add_str(jsb, "delays.");
          js_builder_add_str(jsb, symbol_expression->name);
        }

        ref = ref->next;
      }

      js_builder_end_object(jsb);
    }

    if(has_service) {
      js_builder_start_prop(jsb, "services");
      js_builder_start_object(jsb);

      Ref* ref = printer->service_refs;
      while(ref != NULL) {
        switch(ref->value->type) {
          case EXPRESSION_INVOKE: {
            InvokeExpression* invoke_expression = (InvokeExpression*)ref->value;
            switch(invoke_expression->ref->type) {
              case EXPRESSION_IDENTIFIER: {
                printer_warn(printer, LUCY_STREAM_STDERR, "Currently not supported: TODO");
                break;
              }
              case EXPRESSION_SYMBOL: {
                SymbolExpression* symbol_expression = (SymbolExpression*)invoke_expression->ref;
                js_builder_start_prop(jsb, symbol_expression->name);
                js_builder_add_str(jsb, "services.");
                js_builder_add_str(jsb, symbol_expression->name);
                break;
              }
              default: {
                break;
              }
            }
            break;
          }
          default: {
            break;
          }
        }

        ref = ref->next;
      }

      js_builder_end_object(jsb);
    }

    js_builder_end_object(jsb);
  } else if(!is_nested) {
    js_builder_end_object(jsb);
  }

  if(!is_nested) {
    js_builder_end_call(jsb);
    js_builder_add_str(jsb, ";\n}");
    js_builder_decrease_indent(jsb);
  }

  if(printer->ts_printer != NULL) {
    ts_printer_figure_out_entry_events(printer->ts_printer);
    ts_printer_create_machine(printer->ts_printer);
  }

  str_set_clear(&printer->guard_names);
  str_set_clear(&printer->action_names);
  str_set_clear(&printer->delay_names);
  str_set_clear(&printer->service_names);
  printer_clear_refs(printer);
}