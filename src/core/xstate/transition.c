#include "printer.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../support/mem.h"
#include "../support/str_builder.h"

void printer_compile_transition_action(Printer* printer, JSBuilder* jsb,
  TransitionAction* action, const char* actions_property) {
  TransitionAction* inner = action;
  bool use_multiline = false;
  while(inner && !use_multiline) {
    if(action->expression && (action->expression->type == EXPRESSION_ASSIGN ||
                               action->expression->type == EXPRESSION_SEND)) {
      use_multiline = true;
      break;
    }
    inner = inner->next;
  }
  js_builder_start_prop(jsb, actions_property);
  js_builder_start_array(jsb, use_multiline);

  while(true) {
    if(action->name != NULL) {
      if(use_multiline) {
        js_builder_add_indent(jsb);
      }
      js_builder_add_string(jsb, action->name);
    } else {
      // Inline assign!
      ExpressionType expression_type = action->expression->type;
      switch(expression_type) {
        case EXPRESSION_ASSIGN: {
          AssignExpression* assign_expression = (AssignExpression*)action->expression;
          if(printer->ts_printer != NULL) {
            ts_printer_add_data(printer->ts_printer, assign_expression->key);
          }

          if(assign_expression->value != NULL) {
            switch(assign_expression->value->type) {
              case EXPRESSION_SYMBOL: {
                SymbolExpression* symbol_expression = (SymbolExpression*)assign_expression->value;
                js_builder_add_string(jsb, symbol_expression->name);
                printer_add_action_ref(printer, symbol_expression->name, (Expression*)assign_expression);

                if(printer->ts_printer != NULL) {
                  ts_printer_add_assign(printer->ts_printer, assign_expression->key, symbol_expression->name, printer->current_event);
                }
                break;
              }
              case EXPRESSION_IDENTIFIER: {
                printer_warn(printer, LUCY_STREAM_STDERR, "Not supported at this time\n");
                break;
              }
              case EXPRESSION_SPAWN: {
                SpawnExpression* spawn_expression = (SpawnExpression*)assign_expression->value;
                if(spawn_expression->target->type == EXPRESSION_SYMBOL) {
                  SymbolExpression* symbol_expression = (SymbolExpression*)spawn_expression->target;
                  if(use_multiline) {
                    js_builder_add_indent(jsb);
                  }
                  // "spawn" + the symbol with its first letter upper-cased.
                  const char* symbol_name = symbol_expression->name;
                  size_t symbol_len = strlen(symbol_name);
                  char* action_name = arena_alloc(printer->arena, size_add(symbol_len, sizeof("spawn")));
                  memcpy(action_name, "spawn", 5);
                  memcpy(action_name + 5, symbol_name, symbol_len);
                  action_name[5] = (char)toupper((unsigned char)action_name[5]);
                  js_builder_add_string(jsb, action_name);
                  printer_add_action_ref(printer, action_name, (Expression*)assign_expression);

                  if(printer->ts_printer != NULL) {
                    ts_printer_add_actor(printer->ts_printer, symbol_expression->name);
                  }
                } else {
                  printer_start_assign_call(jsb, assign_expression);
                  printer_add_spawn_call(printer, jsb, spawn_expression);
                  printer_end_assign_call(jsb);
                }
                break;
              }
              default: {
                break;
              }
            }
          } else {
            printer_start_assign_call(jsb, assign_expression);
            js_builder_add_str(jsb, "(context, event) => event.data");
            printer_end_assign_call(jsb);
          }

          break;
        }
        case EXPRESSION_ACTION: {
          ActionExpression* action_expression = (ActionExpression*)action->expression;
          if(use_multiline) {
            js_builder_add_indent(jsb);
          }
          if(action_expression->ref->type == EXPRESSION_IDENTIFIER) {
            IdentifierExpression* expr = (IdentifierExpression*)action_expression->ref;
            js_builder_add_str(jsb, expr->name);
          } else if(action_expression->ref->type == EXPRESSION_SYMBOL) {
            SymbolExpression* expr = (SymbolExpression*)action_expression->ref;
            js_builder_add_string(jsb, expr->name);
            printer_add_action_ref(printer, expr->name, action_expression->ref);

            if(printer->ts_printer != NULL) {
              if(printer->entry_state != NULL) {
                SourceSpan name = printer->entry_state->name;
                char* cur_state_name = arena_strndup(printer->arena,
                  printer->source + name.start, name.end - name.start);
                ts_printer_add_entry_action(printer->ts_printer, cur_state_name, expr->name);
              } else {
                ts_printer_add_action(printer->ts_printer, expr->name, printer->current_event);
              }
            }
          }

          break;
        }
        case EXPRESSION_SEND: {
          if(use_multiline) {
            js_builder_add_indent(jsb);
          }
          SendExpression* send_expression = (SendExpression*)action->expression;
          printer_add_send_call(jsb, send_expression);
          break;
        }
        default: {
          break;
        }
      }
    }

    action = action->next;

    if(action == NULL) {
      break;
    }

    js_builder_add_str(jsb, ", ");
    if(use_multiline) {
      js_builder_add_str(jsb, "\n");
    }
  }

  js_builder_end_array(jsb, use_multiline);
}

void printer_compile_transition_key(Printer* printer, JSBuilder* jsb, TransitionNode* transition_node, const char* event_name) {
  TransitionType type = transition_node->type;

  Node* parent_node = transition_node->node.parent;

  if(parent_node->type == NODE_INVOKE) {
    if(strcmp(event_name, "done") == 0) {
      js_builder_start_prop(jsb, "onDone");
    } else if(strcmp(event_name, "error") == 0) {
      js_builder_start_prop(jsb, "onError");
    } else {
      printer_warnf(printer, LUCY_STREAM_STDOUT, "Regular events in invoke are not supported. Saw event [%s]\n", event_name);
    }
  } else {
    switch(type) {
      case TRANSITION_EVENT: {
        js_builder_start_prop(jsb, event_name);

        // If there is another event with this name, use an array.
        if(transition_node->link != NULL) {
          js_builder_start_array(jsb, true);
          js_builder_add_indent(jsb);
        }

        break;
      }
      case TRANSITION_DELAY: {
        DelayExpression* delay = transition_node->delay->expression;
        if(delay->ref == NULL) {
          char str[12]; // Fits any int, sign and NUL included.
          snprintf(str, sizeof(str), "%i", delay->time);
          js_builder_start_prop(jsb, str);
        } else {
          if(delay->ref->type == EXPRESSION_IDENTIFIER) {
            IdentifierExpression* identifier_expression = (IdentifierExpression*)delay->ref;
            printer_add_delay_ref(printer, identifier_expression->name, (Expression*)delay);
            js_builder_start_prop(jsb, identifier_expression->name);
          } else if(delay->ref->type == EXPRESSION_SYMBOL) {
            SymbolExpression* symbol_expression = (SymbolExpression*)delay->ref;
            printer_add_delay_ref(printer, symbol_expression->name, (Expression*)delay);
            js_builder_start_prop(jsb, symbol_expression->name);

            if(printer->ts_printer != NULL) {
              ts_printer_add_delay(printer->ts_printer, symbol_expression->name);
            }
          }
        }

        break;
      }
      default: {
        break;
      }
    }
  }
}

static inline void compile_guard_expression(Printer* printer, JSBuilder* jsb, GuardExpression* guard_expression) {
  Expression* ref = guard_expression->ref;
  if(ref->type == EXPRESSION_IDENTIFIER) {
    const char* value = ((IdentifierExpression*)ref)->name;
    js_builder_add_str(jsb, value);
  } else if(ref->type == EXPRESSION_SYMBOL) {
    SymbolExpression* symbol_expression = (SymbolExpression*)ref;
    js_builder_add_string(jsb, symbol_expression->name);
    printer_add_guard_ref(printer, symbol_expression->name, ref);
    if(printer->ts_printer != NULL) {
      if(printer->current_event != NULL) {
        ts_printer_add_guard(printer->ts_printer, symbol_expression->name, printer->current_event);
      }
    }
  }
}

// The destination as text. Member expressions are joined into an arena
// string; plain names are returned as they are.
static const char* serialize_destination(Printer* printer, TransitionNode* transition_node) {
  Expression* dest = transition_node->dest;
  switch(dest->type) {
    case EXPRESSION_IDENTIFIER: {
      return ((IdentifierExpression*)dest)->name;
    }
    case EXPRESSION_MEMBER: {
      MemberExpression* member_expression = (MemberExpression*)dest;
      StrBuilder* sb_name = str_builder_create();
      str_builder_add_str(sb_name, member_expression->owner);
      while(member_expression->property->type != EXPRESSION_IDENTIFIER) {
        member_expression = (MemberExpression*)member_expression->property;
        str_builder_add_char(sb_name, '.');
        str_builder_add_str(sb_name, member_expression->owner);
      }
      IdentifierExpression* prop = (IdentifierExpression*)member_expression->property;
      str_builder_add_char(sb_name, '.');
      str_builder_add_str(sb_name, prop->name);
      char* d_name = arena_strdup(printer->arena, str_builder_peek(sb_name));
      str_builder_destroy(sb_name);
      return d_name;
    }
    default: {
      return NULL;
    }
  }
}

void printer_compile_inner_transition(Printer* printer, JSBuilder* jsb, TransitionNode* transition_node) {
  bool is_always = transition_node->type == TRANSITION_IMMEDIATE;
  bool has_guard = transition_node->guard != NULL;
  bool has_action = transition_node->action != NULL;
  bool has_guard_or_action = has_guard || has_action;
  bool use_object_notation = has_guard_or_action || is_always;

  if(use_object_notation) {
    js_builder_start_object(jsb);

    if(transition_node->dest != NULL) {
      js_builder_start_prop(jsb, "target");
      js_builder_add_string(jsb, serialize_destination(printer, transition_node));

      if(printer->ts_printer != NULL) {
        if(printer->current_event != NULL) {
          ts_printer_add_state_entry(printer->ts_printer, printer->current_event, serialize_destination(printer, transition_node));
        }
      }
    }

    if(has_guard) {
      js_builder_start_prop(jsb, "cond");

      TransitionGuard* guard = transition_node->guard;
      // If there are multiple guards use an array.
      if(guard->next) {
        js_builder_start_array(jsb, false);
        while(true) {
          if(guard->name != NULL) {
            js_builder_add_string(jsb, guard->name);
          } else {
            // Expression!
            compile_guard_expression(printer, jsb, guard->expression);
          }

          guard = guard->next;

          if(guard == NULL) {
            break;
          }

          js_builder_add_str(jsb, ", ");
        }

        js_builder_end_array(jsb, false);
      }
      // If a single guard use a string
      else {
        if(guard->name != NULL) {
          js_builder_add_string(jsb, guard->name);
        } else {
          // Expression!
          compile_guard_expression(printer, jsb, guard->expression);
        }
      }
    }

    if(has_action) {
      printer_compile_transition_action(printer, jsb, transition_node->action, "actions");
    }

    js_builder_end_object(jsb);
  } else {
    js_builder_add_string(jsb, serialize_destination(printer, transition_node));
  }
}

void printer_compile_event_transition(Printer* printer, JSBuilder* jsb, TransitionNode* transition_node) {
  const char* event_name = transition_node->event->type == EXPRESSION_ON ?
                             ((OnExpression*)transition_node->event)->name :
                             ((IdentifierExpression*)transition_node->event)->name;

  // If this transition was already compiled from a previous link.
  if(str_set_contains(&printer->event_names, event_name)) {
    return;
  }

  printer->current_event = event_name;
  if(printer->ts_printer != NULL) {
    ts_printer_add_event(printer->ts_printer, event_name);
  }
  printer_compile_transition_key(printer, jsb, transition_node, event_name);

  printer_compile_inner_transition(printer, jsb, transition_node);
  TransitionNode* cur = transition_node->link;
  while(cur != NULL) {
    js_builder_add_str(jsb, ", \n");
    js_builder_add_indent(jsb);
    printer_compile_inner_transition(printer, jsb, cur);
    cur = cur->link;
  }

  if(transition_node->link != NULL) {
    js_builder_end_array(jsb, true);
  }

  printer->current_event = NULL;
  str_set_add(&printer->event_names, event_name);
}
