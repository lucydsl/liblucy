#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "../js_builder.h"
#include "../node.h"
#include "../set.h"
#include "core.h"
#include "ts_printer.h"

static void add_machine_fn_args(PrintState* state, JSBuilder* jsb, MachineNode* machine_node) {
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

void xs_add_machine_binding_name(JSBuilder* jsb, MachineNode* machine_node) {
  js_builder_add_str(jsb, "create");
  char* machine_name = machine_node->name;
  int machine_name_len = strlen(machine_name);
  js_builder_add_char(jsb, toupper(machine_name[0]));
  int i = 1;
  while(i < machine_name_len) {
    js_builder_add_char(jsb, machine_name[i]);
    i++;
  }
}

static inline void compile_machine(PrintState* state, JSBuilder* jsb, MachineNode* machine_node, bool is_nested) {
  if(!is_nested) {
    if(machine_node->name == NULL) {
      js_builder_add_str(jsb, "\nexport default function(");
      add_machine_fn_args(state, jsb, machine_node);
      js_builder_add_str(jsb, ") {\n");
      js_builder_increase_indent(jsb);
    } else {
      js_builder_add_export(jsb);
      js_builder_add_str(jsb, "function ");
      xs_add_machine_binding_name(jsb, machine_node);
      js_builder_add_str(jsb, "(");
      add_machine_fn_args(state, jsb, machine_node);
      js_builder_add_str(jsb, ") {\n");
      js_builder_increase_indent(jsb);
    }
    js_builder_start_call(jsb, "return createMachine");
    js_builder_start_object(jsb);
  }

  if(machine_node->initial != NULL) {
    js_builder_start_prop(jsb, "initial");
    js_builder_add_string(jsb, machine_node->initial);
  }
  js_builder_shorthand_prop(jsb, "context");
}

void xs_enter_machine(PrintState* state, JSBuilder* jsb, Node* node) {
  MachineNode *machine_node = (MachineNode*)node;
  Node* parent_node = node->parent;
  bool is_nested = node_machine_is_nested(node);
  compile_machine(state, jsb, machine_node, is_nested);
  if(state->flags & XS_FLAG_DTS) {
    ts_printer_add_machine_name(state->tsprinter, machine_node->name);
  }
}

void xs_exit_machine(PrintState* state, JSBuilder* jsb, Node* node) {
  bool has_guard = state->guard != NULL;
  bool has_action = state->action != NULL;
  bool has_delay = state->delay != NULL;
  bool has_service = state->service != NULL;
  bool needs_options = has_guard || has_action || has_delay || has_service;
  bool is_nested = node_machine_is_nested(node);

  if(!is_nested && needs_options) {
    js_builder_end_object(jsb);
    js_builder_add_str(jsb, ", ");

    js_builder_start_object(jsb);

    if(has_guard) {
      js_builder_start_prop(jsb, "guards");
      js_builder_start_object(jsb);

      Ref* ref = state->guard;
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
            printf("Unexpected type of expression\n");
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

      Ref* ref = state->action;
      while(ref != NULL) {
        js_builder_start_prop(jsb, ref->key);

        Expression* expression = ref->value;

        switch(expression->type) {
          case EXPRESSION_ASSIGN: {
            AssignExpression* assign = (AssignExpression*)expression;

            xs_start_assign_call(jsb, assign);

            char* identifier;
            switch(assign->value->type) {
              case EXPRESSION_IDENTIFIER: {
                char* identifier = ((IdentifierExpression*)assign->value)->name;
                js_builder_add_str(jsb, identifier);
                break;
              }
              case EXPRESSION_SPAWN: {
                SpawnExpression* spawn_expression = (SpawnExpression*)assign->value;
                xs_add_spawn_call(state, jsb, spawn_expression);
                break;
              }
              case EXPRESSION_SYMBOL: {
                SymbolExpression* symbol_expression = (SymbolExpression*)assign->value;
                js_builder_add_str(jsb, "assigns.");
                js_builder_add_str(jsb, symbol_expression->name);
                break;
              }
            }

            xs_end_assign_call(jsb);
            break;
          }
          case EXPRESSION_SEND: {
            SendExpression* send_expression = (SendExpression*)expression;
            xs_add_send_call(jsb, send_expression);
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
            printf("This type of expression is not currently supported.\n");
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

      Ref* ref = state->delay;
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

      Ref* ref = state->service;
      while(ref != NULL) {
        switch(ref->value->type) {
          case EXPRESSION_INVOKE: {
            InvokeExpression* invoke_expression = (InvokeExpression*)ref->value;
            switch(invoke_expression->ref->type) {
              case EXPRESSION_IDENTIFIER: {
                fprintf(stderr, "Currently not supported: TODO");
                break;
              }
              case EXPRESSION_SYMBOL: {
                SymbolExpression* symbol_expression = (SymbolExpression*)invoke_expression->ref;
                js_builder_start_prop(jsb, symbol_expression->name);
                js_builder_add_str(jsb, "services.");
                js_builder_add_str(jsb, symbol_expression->name);
                break;
              }
            }
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

  if(state->flags & XS_FLAG_DTS) {
    ts_printer_figure_out_entry_events(state->tsprinter);
    ts_printer_create_machine(state->tsprinter);
  }

  set_clear(state->guard_names);
  set_clear(state->action_names);
  set_clear(state->delay_names);
  set_clear(state->service_names);
  xs_destroy_state_refs(state);
}