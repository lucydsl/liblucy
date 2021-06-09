#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "../js_builder.h"
#include "../set.h"
#include "../str_builder.h"
#include "../node.h"
#include "core.h"

void xs_compile_transition_action(PrintState* state, JSBuilder* jsb, TransitionAction* action, const char* actions_property) {
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
  js_builder_start_prop(jsb, (char*)actions_property);
  js_builder_start_array(jsb, use_multiline);

  while(true) {
    if(action->name != NULL) {
      if(use_multiline) {
        js_builder_add_indent(jsb);
      }
      js_builder_add_string(jsb, action->name);
    } else {
      // Inline assign!
      unsigned short expression_type = action->expression->type;
      switch(expression_type) {
        case EXPRESSION_ASSIGN: {
          AssignExpression* assign_expression = (AssignExpression*)action->expression;

          if(assign_expression->value != NULL) {
            switch(assign_expression->value->type) {
              case EXPRESSION_SYMBOL: {
                SymbolExpression* symbol_expression = (SymbolExpression*)assign_expression->value;
                js_builder_add_string(jsb, symbol_expression->name);
                xs_add_action_ref(state, symbol_expression->name, (Expression*)assign_expression);
                break;
              }
              case EXPRESSION_IDENTIFIER: {
                fprintf(stderr, "Not supported at this time\n"); // TODO
                break;
              }
              case EXPRESSION_SPAWN: {
                SpawnExpression* spawn_expression = (SpawnExpression*)assign_expression->value;
                if(spawn_expression->target->type == EXPRESSION_SYMBOL) {
                  SymbolExpression* symbol_expression = (SymbolExpression*)spawn_expression->target;
                  if(use_multiline) {
                    js_builder_add_indent(jsb);
                  }
                  str_builder_t* sb_name = str_builder_create();
                  str_builder_add_str(sb_name, "spawn", 5);
                  str_builder_add_char(sb_name, toupper(symbol_expression->name[0]));
                  char* t = symbol_expression->name;
                  t++;
                  for(; *t != '\0'; t++) {
                    str_builder_add_char(sb_name, *t);
                  }
                  char* action_name = str_builder_dump(sb_name, 0);
                  str_builder_destroy(sb_name);
                  js_builder_add_string(jsb, action_name);
                  xs_add_action_ref(state, action_name, (Expression*)assign_expression);
                } else {
                  xs_start_assign_call(jsb, assign_expression);
                  xs_add_spawn_call(state, jsb, spawn_expression);
                  xs_end_assign_call(jsb);
                }
                break;
              }
            }
          } else {
            xs_start_assign_call(jsb, assign_expression);
            js_builder_add_str(jsb, "(context, event) => event.data");
            xs_end_assign_call(jsb);
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
            xs_add_action_ref(state, expr->name, action_expression->ref);
          }
          
          break;
        }
        case EXPRESSION_SEND: {
          if(use_multiline) {
            js_builder_add_indent(jsb);
          }
          SendExpression* send_expression = (SendExpression*)action->expression;
          xs_add_send_call(jsb, send_expression);
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

void xs_compile_transition_key(PrintState* state, JSBuilder* jsb, Node* node, char* event_name) {
  TransitionNode* transition_node = (TransitionNode*)node;
  int type = transition_node->type;

  Node* parent_node = node->parent;

  if(parent_node->type == NODE_INVOKE_TYPE) {
    if(strcmp(event_name, "done") == 0) {
      js_builder_start_prop(jsb, "onDone");
    } else if(strcmp(event_name, "error") == 0) {
      js_builder_start_prop(jsb, "onError");
    } else {
      printf("Regular events in invoke are not supported. Saw event [%s]\n", event_name);
    }
  } else {
    switch(type) {
      case TRANSITION_EVENT_TYPE: {
        js_builder_start_prop(jsb, event_name);

        // If there is another event with this name, use an array.
        if(transition_node->link != NULL) {
          js_builder_start_array(jsb, true);
          js_builder_add_indent(jsb);
        }

        break;
      }
      case TRANSITION_DELAY_TYPE: {
        DelayExpression* delay = transition_node->delay->expression;
        if(delay->ref == NULL) {
          int ms = delay->time;
          int length = (int)((ceil(log10(ms))+1)*sizeof(char));
          char str[length];
          sprintf(str, "%i", ms);
          js_builder_start_prop(jsb, str);
        } else {
          if(delay->ref->type == EXPRESSION_IDENTIFIER) {
            IdentifierExpression* identifier_expression = (IdentifierExpression*)delay->ref;
            xs_add_delay_ref(state, identifier_expression->name, (Expression*)delay);
            js_builder_start_prop(jsb, identifier_expression->name);
          } else if(delay->ref->type == EXPRESSION_SYMBOL) {
            SymbolExpression* symbol_expression = (SymbolExpression*)delay->ref;
            xs_add_delay_ref(state, symbol_expression->name, (Expression*)delay);
            js_builder_start_prop(jsb, symbol_expression->name);
          }
        }

        break;
      }
    }
  }
}

static inline void compile_guard_expression(PrintState* state, JSBuilder* jsb, GuardExpression* guard_expression) {
  Expression* ref = guard_expression->ref;
  if(ref->type == EXPRESSION_IDENTIFIER) {
    char* value = ((IdentifierExpression*)ref)->name;
    js_builder_add_str(jsb, value);
  } else if(ref->type == EXPRESSION_SYMBOL) {
    SymbolExpression* symbol_expression = (SymbolExpression*)ref;
    js_builder_add_string(jsb, symbol_expression->name);
    xs_add_guard_ref(state, symbol_expression->name, ref);
  }
}

void xs_compile_inner_transition(PrintState* state, JSBuilder* jsb, TransitionNode* transition_node) {
  bool is_always = transition_node->type == TRANSITION_IMMEDIATE_TYPE;
  bool has_guard = transition_node->guard != NULL;
  bool has_action = transition_node->action != NULL;
  bool has_guard_or_action = has_guard || has_action;
  bool use_object_notation = has_guard_or_action || is_always;

  if(use_object_notation) {
    js_builder_start_object(jsb);

    if(transition_node->dest != NULL) {
      js_builder_start_prop(jsb, "target");
      js_builder_add_string(jsb, transition_node->dest);
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
            compile_guard_expression(state, jsb, guard->expression);
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
          compile_guard_expression(state, jsb, guard->expression);
        }
      }
    }

    if(has_action) {
      xs_compile_transition_action(state, jsb, transition_node->action, "actions");
    }

    js_builder_end_object(jsb);
  } else {
    js_builder_add_string(jsb, transition_node->dest);
  }
}

void xs_compile_event_transition(PrintState* state, JSBuilder* jsb, TransitionNode* transition_node) {
  char* event_name = transition_node->event->type == EXPRESSION_ON ?
    ((OnExpression*)transition_node->event)->name :
    ((IdentifierExpression*)transition_node->event)->name;

  // If this transition was already compiled from a previous link.
  if(event_name != NULL && set_contains(state->events, event_name) == SET_TRUE) {
    return;
  }

  xs_compile_transition_key(state, jsb, (Node*)transition_node, event_name);

  xs_compile_inner_transition(state, jsb, transition_node);
  TransitionNode* cur = transition_node->link;
  while(cur != NULL) {
    js_builder_add_str(jsb, ", \n");
    js_builder_add_indent(jsb);
    xs_compile_inner_transition(state, jsb, cur);
    cur = cur->link;
  }

  if(transition_node->link != NULL) {
    js_builder_end_array(jsb, true);
  }

  set_add(state->events, event_name);
}