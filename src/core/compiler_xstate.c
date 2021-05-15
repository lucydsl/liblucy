#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "dict.h"
#include "node.h"
#include "program.h"
#include "parser.h"
#include "str_builder.h"
#include "js_builder.h"
#include "compiler_xstate.h"
#include "local.h"
#include "set.h"

// API flags
#define FLAG_USE_REMOTE 1 << 0

// Machine implementation flags
#define XS_HAS_STATE_PROP 1 << 0

typedef struct Ref {
  char* key;
  struct Ref* next;
  Expression* value;
} Ref;

typedef struct PrintState {
  bool on_prop_added;
  bool always_prop_added;
  Program* program;
  Ref* guard;
  SimpleSet* guard_names;
  Ref* action;
  SimpleSet* action_names;
  Ref* delay;
  SimpleSet* delay_names;
  Ref* service;
  SimpleSet* service_names;
  SimpleSet* events;
} PrintState;

typedef void (*set_ref)(PrintState*, Ref*);
static void compile_transition_action(PrintState*, JSBuilder*, TransitionAction*, const char*);
static void compile_local_node(PrintState*, JSBuilder*, LocalNode*);
static bool find_and_add_top_level_machine_name(PrintState*, JSBuilder*, char*);

static void add_ref(PrintState* state, char* key, Expression* value, Ref* head, SimpleSet* set, set_ref setter) {
  // If already added, don't do so again.
  if(set_contains(set, key) == SET_TRUE) {
    return;
  }

  Ref *ref = malloc(sizeof(Ref));
  ref->key = strdup(key);
  ref->value = node_clone_expression(value);
  ref->next = NULL;

  if(head == NULL) {
    setter(state, ref);
  } else {
    Ref* cur = head; 
    while(cur->next != NULL) {
      cur = cur->next;
    }
    cur->next = ref;
  }
  set_add(set, key);
}

static void set_action_ref(PrintState* state, Ref* ref) {
  state->action = ref;
}

static void add_action_ref(PrintState* state, char* key, Expression* value) {
  add_ref(state, key, value, state->action, state->action_names, *set_action_ref);
}

static void set_guard_ref(PrintState* state, Ref* ref) {
  state->guard = ref;
}

static void add_guard_ref(PrintState* state, char* key, Expression* value) {
  add_ref(state, key, value, state->guard, state->guard_names, *set_guard_ref);
}

static void set_delay_ref(PrintState* state, Ref* ref) {
  state->delay = ref;
}

static void add_delay_ref(PrintState* state, char* key, Expression* value) {
  add_ref(state, key, value, state->delay, state->delay_names, *set_delay_ref);
}

static void set_service_ref(PrintState* state, Ref* ref) {
  state->service = ref;
}

static void add_service_ref(PrintState* state, char* key, Expression* value) {
  add_ref(state, key, value, state->service, state->service_names, *set_service_ref);
}

static void destroy_ref(Ref* ref) {
  if(ref->next != NULL) {
    destroy_ref(ref->next);
  }
  if(ref->key != NULL) {
    free(ref->key);
  }
  if(ref->value != NULL) {
    node_destroy_expression(ref->value);
  }
  free(ref);
}

static void destroy_state(PrintState *state) {
  if(state->guard != NULL) {
    destroy_ref(state->guard);
  }
  if(state->action != NULL) {
    destroy_ref(state->action);
  }
  if(state->delay != NULL) {
    destroy_ref(state->delay);
  }
  set_destroy(state->events);
  set_destroy(state->action_names);
  set_destroy(state->guard_names);
  set_destroy(state->delay_names);
  set_destroy(state->service_names);
}

static void start_assign_call(JSBuilder* jsb, AssignExpression* assign_expression) {
  js_builder_start_call(jsb, "assign");
  js_builder_start_object(jsb);
  js_builder_start_prop(jsb, assign_expression->key);
}

static void end_assign_call(JSBuilder* jsb) {
  js_builder_end_object(jsb);
  js_builder_end_call(jsb);
}

static void add_spawn_call(PrintState* state, JSBuilder* jsb, SpawnExpression* spawn_expression) {
  js_builder_start_call(jsb, "spawn");
  Expression* target = spawn_expression->target;
  char* name = NULL;
  if(target->type == EXPRESSION_IDENTIFIER) {
    name = ((IdentifierExpression*)target)->name;
    if(!find_and_add_top_level_machine_name(state, jsb, name)) {
      js_builder_add_str(jsb, name);
    }
  } else {
    name = ((SymbolExpression*)target)->name;
    js_builder_add_str(jsb, "services.");
    js_builder_add_str(jsb, name);
  }
  js_builder_add_str(jsb, ", ");
  js_builder_add_string(jsb, name);
  js_builder_end_call(jsb);
}

static void add_send_call(JSBuilder* jsb, SendExpression* send_expression) {
  js_builder_start_call(jsb, "send");
  js_builder_add_string(jsb, send_expression->event);
  js_builder_add_str(jsb, ", ");
  js_builder_start_object(jsb);
  js_builder_start_prop(jsb, "to");
  js_builder_add_str(jsb, "(context) => context.");
  js_builder_add_str(jsb, send_expression->actor);
  js_builder_end_object(jsb);
  js_builder_end_call(jsb);
}

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

static void add_machine_binding_name(JSBuilder* jsb, MachineNode* machine_node) {
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

static void enter_machine(PrintState* state, JSBuilder* jsb, Node* node) {
  MachineNode *machine_node = (MachineNode*)node;
  Node* parent_node = node->parent;
  bool is_nested = node_machine_is_nested(node);

  if(!is_nested) {
    if(machine_node->name == NULL) {
      js_builder_add_str(jsb, "\nexport default function(");
      add_machine_fn_args(state, jsb, machine_node);
      js_builder_add_str(jsb, ") {\n");
      js_builder_increase_indent(jsb);
    } else {
      js_builder_add_export(jsb);
      js_builder_add_str(jsb, "function ");
      add_machine_binding_name(jsb, machine_node);
      js_builder_add_str(jsb, "(");
      add_machine_fn_args(state, jsb, machine_node);
      js_builder_add_str(jsb, ") {\n");
      js_builder_increase_indent(jsb);
    }
    js_builder_start_call(jsb, "return createMachine");
    js_builder_start_object(jsb);
  } else {
    // Close out the on prop
    if(state->on_prop_added) {
      state->on_prop_added = false;
      js_builder_end_object(jsb);
    }
  }

  if(machine_node->initial != NULL) {
    js_builder_start_prop(jsb, "initial");
    js_builder_add_string(jsb, machine_node->initial);
  }
  js_builder_shorthand_prop(jsb, "context");
}

static void exit_machine(PrintState* state, JSBuilder* jsb, Node* node) {
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

            start_assign_call(jsb, assign);

            char* identifier;
            switch(assign->value->type) {
              case EXPRESSION_IDENTIFIER: {
                char* identifier = ((IdentifierExpression*)assign->value)->name;
                js_builder_add_str(jsb, identifier);
                break;
              }
              case EXPRESSION_SPAWN: {
                SpawnExpression* spawn_expression = (SpawnExpression*)assign->value;
                add_spawn_call(state, jsb, spawn_expression);
                break;
              }
              case EXPRESSION_SYMBOL: {
                SymbolExpression* symbol_expression = (SymbolExpression*)assign->value;
                js_builder_add_str(jsb, "assigns.");
                js_builder_add_str(jsb, symbol_expression->name);
                break;
              }
            }

            end_assign_call(jsb);
           // js_builder_end_object(jsb);
           // js_builder_end_call(jsb);
            break;
          }
          case EXPRESSION_SEND: {
            SendExpression* send_expression = (SendExpression*)expression;
            add_send_call(jsb, send_expression);
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

  set_clear(state->guard_names);
  set_clear(state->action_names);
  set_clear(state->delay_names);
  set_clear(state->service_names);
}

static void enter_import(PrintState* state, JSBuilder* jsb, Node* node) {
  js_builder_add_str(jsb, "import ");

  ImportNode *import_node = (ImportNode*)node;
  Node* child = node->child;
  bool multiple = false;

  if(child == NULL) {
    printf("TODO add support for imports with no specifiers\n");
    return;
  }

  js_builder_add_str(jsb, "{ ");

  while(child != NULL) {
    ImportSpecifier *specifier = (ImportSpecifier*)child;

    if(multiple) {
      js_builder_add_str(jsb, ", ");
    }

    js_builder_add_str(jsb, specifier->imported);
    // TODO support local

    child = child->next;
    multiple = true;
  }

  js_builder_add_str(jsb, " } from ");
  js_builder_add_str(jsb, import_node->from);
  js_builder_add_str(jsb, ";\n");
}

static void enter_state(PrintState* state, JSBuilder* jsb, Node* node) {
  StateNode* state_node = (StateNode*)node;
  MachineNode* machine_node = (MachineNode*)node->parent;
  if(!(machine_node->impl_flags & XS_HAS_STATE_PROP)) {
    machine_node->impl_flags |= XS_HAS_STATE_PROP;
    js_builder_start_prop(jsb, "states");
    js_builder_start_object(jsb);
  }

  js_builder_start_prop(jsb, state_node->name);
  js_builder_start_object(jsb);

  if(state_node->final) {
    js_builder_start_prop(jsb, "type");
    js_builder_add_string(jsb, "final");
  }

  if(state_node->entry != NULL) {
    compile_local_node(state, jsb, state_node->entry);
  }
  if(state_node->exit != NULL) {
    compile_local_node(state, jsb, state_node->exit);
  }
}

static void compile_local_node(PrintState* state, JSBuilder* jsb, LocalNode* local_node) {
  switch(local_node->key) {
    case LOCAL_ENTRY: {
      compile_transition_action(state, jsb, local_node->action, "entry");
      break;
    }
    case LOCAL_EXIT: {
      compile_transition_action(state, jsb, local_node->action, "exit");
      break;
    }
  }
}

static void exit_state(PrintState* state, JSBuilder* jsb, Node* node) {
  StateNode* state_node = (StateNode*)node;

  // Close out this state node.
  js_builder_end_object(jsb);

  // End of all state nodes
  if(!node->next || node->next->type != NODE_STATE_TYPE) {
    js_builder_end_object(jsb);
  }
  set_clear(state->events);
}

static bool find_and_add_top_level_machine_name(PrintState* state, JSBuilder* jsb, char* matching_name) {
  unsigned long searchid = hash_function(matching_name);
  Node* cur = state->program->body;
  while(cur != NULL) {
    if(cur->type == NODE_MACHINE_TYPE) {
      MachineNode* cur_machine = (MachineNode*)cur;
      char* machine_name = cur_machine->name;
      if(machine_name != NULL && hash_function(machine_name) == searchid) {
        add_machine_binding_name(jsb, cur_machine);
        return true;
      }
    }
    cur = cur->next;
  }
  return false;
}

static void enter_invoke(PrintState* state, JSBuilder* jsb, Node* node) {
  js_builder_start_prop(jsb, "invoke");
  js_builder_start_object(jsb);

  InvokeNode* invoke_node = (InvokeNode*)node;
  js_builder_start_prop(jsb, "src");

  InvokeExpression* invoke_expression = (InvokeExpression*)invoke_node->expr;
  switch(invoke_expression->ref->type) {
    case EXPRESSION_IDENTIFIER: {
      IdentifierExpression* identifier_expression = (IdentifierExpression*)invoke_expression->ref;

      // Looking for a machine matching this name.
      if(!find_and_add_top_level_machine_name(state, jsb, identifier_expression->name)) {
        // No in-scope machine found, so just add the identifier directly.
        js_builder_add_str(jsb, identifier_expression->name);
      }
      break;
    }
    case EXPRESSION_SYMBOL: {
      SymbolExpression* symbol_expression = (SymbolExpression*)invoke_expression->ref;
      js_builder_add_string(jsb, symbol_expression->name);
      add_service_ref(state, symbol_expression->name, (Expression*)invoke_expression);
      break;
    }
  }
}

static void exit_invoke(PrintState* state, JSBuilder* jsb, Node* node) {
  js_builder_end_object(jsb);
}

static void compile_transition_action(PrintState* state, JSBuilder* jsb, TransitionAction* action, const char* actions_property) {
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
                add_action_ref(state, symbol_expression->name, (Expression*)assign_expression);
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
                  add_action_ref(state, action_name, (Expression*)assign_expression);
                } else {
                  start_assign_call(jsb, assign_expression);
                  add_spawn_call(state, jsb, spawn_expression);
                  end_assign_call(jsb);
                }
                break;
              }
            }
          } else {
            start_assign_call(jsb, assign_expression);
            js_builder_add_str(jsb, "(context, event) => event.data");
            end_assign_call(jsb);
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
            add_action_ref(state, expr->name, action_expression->ref);
          }
          
          break;
        }
        case EXPRESSION_SEND: {
          if(use_multiline) {
            js_builder_add_indent(jsb);
          }
          SendExpression* send_expression = (SendExpression*)action->expression;
          add_send_call(jsb, send_expression);
          break;
        }
      }
    }

    action = action->next;

    if(action == NULL) {
      break;
    }

    js_builder_add_str(jsb, ", ");
  }

  js_builder_end_array(jsb, use_multiline);
}

static inline void compile_transition_key(PrintState* state, JSBuilder* jsb, Node* node, char* event_name) {
  TransitionNode* transition_node = (TransitionNode*)node;
  int type = transition_node->type;

  Node* parent_node = node->parent;

  if(parent_node->type == NODE_INVOKE_TYPE) {
    if(strcmp(event_name, "done") == 0) {
      js_builder_start_prop(jsb, "onDone");
    } else if(strcmp(event_name, "error") == 0) {
      js_builder_start_prop(jsb, "onError");
    } else {
      printf("Regular events in invoke are not supported.\n");
    }
  } else {
    switch(type) {
      case TRANSITION_EVENT_TYPE: {
        if(!state->on_prop_added) {
          state->on_prop_added = true;
          js_builder_start_prop(jsb, "on");
          js_builder_start_object(jsb);
        }

        js_builder_start_prop(jsb, event_name);

        // If there is another event with this name, use an array.
        if(transition_node->link != NULL) {
          js_builder_start_array(jsb, true);
          js_builder_add_indent(jsb);
        }

        break;
      }
      case TRANSITION_IMMEDIATE_TYPE: {
        if(!state->always_prop_added) {
          state->always_prop_added = true;
          js_builder_start_prop(jsb, "always");
          js_builder_start_array(jsb, true);
          js_builder_add_indent(jsb);
        } else {
          js_builder_add_str(jsb, ", ");
        }
        break;
      }
      case TRANSITION_DELAY_TYPE: {
        js_builder_start_prop(jsb, "after");
        js_builder_start_object(jsb);

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
            add_delay_ref(state, identifier_expression->name, (Expression*)delay);
            js_builder_start_prop(jsb, identifier_expression->name);
          } else if(delay->ref->type == EXPRESSION_SYMBOL) {
            SymbolExpression* symbol_expression = (SymbolExpression*)delay->ref;
            add_delay_ref(state, symbol_expression->name, (Expression*)delay);
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
    add_guard_ref(state, symbol_expression->name, ref);
  }
}

static inline void compile_inner_transition(PrintState* state, JSBuilder* jsb, TransitionNode* transition_node) {
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
      compile_transition_action(state, jsb, transition_node->action, "actions");
    }

    js_builder_end_object(jsb);

    if(is_always) {
      if(!node_transition_has_sibling_always(transition_node)) {
        js_builder_end_array(jsb, true);
      }
    }
  } else {
    js_builder_add_string(jsb, transition_node->dest);
  }
}

static void enter_transition(PrintState* state, JSBuilder* jsb, Node* node) {
  TransitionNode* transition_node = (TransitionNode*)node;

  char* event_name = NULL;
  if(transition_node->event != NULL) {
    event_name = transition_node->event->type == EXPRESSION_ON ?
      ((OnExpression*)transition_node->event)->name :
      ((IdentifierExpression*)transition_node->event)->name;
  }

  // If this transition was already compiled from a previous link.
  if(event_name != NULL && set_contains(state->events, event_name) == SET_TRUE) {
    return;
  }

  compile_transition_key(state, jsb, node, event_name);

  compile_inner_transition(state, jsb, transition_node);
  TransitionNode* cur = transition_node->link;
  while(cur != NULL) {
    js_builder_add_str(jsb, ", \n");
    js_builder_add_indent(jsb);
    compile_inner_transition(state, jsb, cur);
    cur = cur->link;
  }

  if(transition_node->link != NULL) {
    js_builder_end_array(jsb, true);
  }

  if(event_name != NULL) {
    set_add(state->events, event_name);
  }
}

static void exit_transition(PrintState* state, JSBuilder* jsb, Node* node) {
  if(node->next) {

  } else {
    TransitionNode* transition_node = (TransitionNode*)node;
    if(state->on_prop_added) {
      state->on_prop_added = false;
      js_builder_end_object(jsb);
    }

    if(state->always_prop_added) {
      state->always_prop_added = false;
    }

    if(transition_node->type == TRANSITION_DELAY_TYPE) {
      js_builder_end_object(jsb);
    }
  }
}

static void enter_assignment(PrintState* state, JSBuilder* jsb, Node* node) {
  Assignment* assignment = (Assignment*)node;

  switch(assignment->binding_type) {
    case ASSIGNMENT_ACTION: {
      add_action_ref(state, assignment->binding_name, assignment->value);
      break;
    }
    case ASSIGNMENT_GUARD: {
      add_guard_ref(state, assignment->binding_name, assignment->value);
      break;
    }
  }
}

CompileResult* xs_create() {
  CompileResult* result = malloc(sizeof(*result));
  return result;
}

void xs_init(CompileResult* result, int use_remote_source) {
  result->success = false;
  result->js = NULL;
  result->flags = 0;
  
  if(use_remote_source) {
    result->flags |= FLAG_USE_REMOTE;
  }
}

void compile_xstate(CompileResult* result, char* source, char* filename) {
  ParseResult *parse_result = parse(source, filename);

  if(parse_result->success == false) {
    result->success = false;
    result->js = NULL;
    return;
  }

  Program *program = parse_result->program;
  char* xstate_specifier;
  if(result->flags & FLAG_USE_REMOTE) {
    xstate_specifier = "https://cdn.skypack.dev/xstate";
  } else {
    xstate_specifier = "xstate";
  }

  JSBuilder *jsb;
  Node* node;
  jsb = js_builder_create();
  node = program->body;

  if(node != NULL) {
    js_builder_add_str(jsb, "import { createMachine");

    if(program->flags & PROGRAM_USES_ASSIGN) {
      js_builder_add_str(jsb, ", assign");
    }
    if(program->flags & PROGRAM_USES_SEND) {
      js_builder_add_str(jsb, ", send");
    }
    if(program->flags & PROGRAM_USES_SPAWN) {
      js_builder_add_str(jsb, ", spawn");
    }

    js_builder_add_str(jsb, " } from '");

    js_builder_add_str(jsb, xstate_specifier);
    js_builder_add_str(jsb, "';\n");
  }

  PrintState state = {
    .program = program,
    .on_prop_added = false,
    .always_prop_added = false,
    .guard = NULL,
    .action = NULL,
    .delay = NULL,
    .events = malloc(sizeof(SimpleSet)),
    .guard_names = malloc(sizeof(SimpleSet)),
    .action_names = malloc(sizeof(SimpleSet)),
    .delay_names = malloc(sizeof(SimpleSet)),
    .service_names = malloc(sizeof(SimpleSet))
  };
  set_init(state.events);
  set_init(state.guard_names);
  set_init(state.action_names);
  set_init(state.delay_names);
  set_init(state.service_names);

  bool exit = false;
  while(node != NULL) {
    unsigned short type = node->type;

    if(exit) {
      switch(type) {
        case NODE_STATE_TYPE: {
          exit_state(&state, jsb, node);
          node_destroy_state((StateNode*)node);
          break;
        }
        case NODE_TRANSITION_TYPE: {
          exit_transition(&state, jsb, node);
          node_destroy_transition((TransitionNode*)node);
          break;
        }
        case NODE_ASSIGNMENT_TYPE: {
          node_destroy_assignment((Assignment*)node);
          break;
        }
        case NODE_IMPORT_SPECIFIER_TYPE: {
          node_destroy_import_specifier((ImportSpecifier*)node);
          break;
        }
        case NODE_IMPORT_TYPE: {
          node_destroy_import((ImportNode*)node);
          break;
        }
        case NODE_MACHINE_TYPE: {
          exit_machine(&state, jsb, node);
          node_destroy_machine((MachineNode*)node);
          break;
        }
        case NODE_INVOKE_TYPE: {
          exit_invoke(&state, jsb, node);
          node_destroy_invoke((InvokeNode*)node);
          break;
        }
        default: {
          printf("Node type %hu not torn down (this is a compiler bug)\n", type);
          break;
        }
      }
    } else {
      switch(type) {
        case NODE_IMPORT_TYPE: {
          enter_import(&state, jsb, node);
          break;
        }
        case NODE_STATE_TYPE: {
          enter_state(&state, jsb, node);
          break;
        }
        case NODE_TRANSITION_TYPE: {
          enter_transition(&state, jsb, node);
          break;
        }
        case NODE_ASSIGNMENT_TYPE: {
          enter_assignment(&state, jsb, node);
          break;
        }
        case NODE_INVOKE_TYPE: {
          enter_invoke(&state, jsb, node);
          break;
        }
        case NODE_LOCAL_TYPE: {
          // Do not walk down local nodes.
          exit = true;
          node_destroy_local((LocalNode*)node);
          break;
        }
        case NODE_MACHINE_TYPE: {
          enter_machine(&state, jsb, node);
          break;
        }
      }
    }

    // Node has no children, so go again for the exit.
    if(!exit && !node->child) {
      exit = true;
      continue;
    }
    // Node has a child
    else if(!exit && node->child) {
      node = node->child;
    } else if(node->next) {
      exit = false;
      node = node->next;
    } else if(node->parent) {
      exit = true;
      node_destroy(node);
      node = node->parent;
    } else {
      // Reached the end.
      node_destroy(node);
      break;
    }
  }

  char* js = js_builder_dump(jsb);
  result->success = true;
  result->js = js;

  // Teardown
  destroy_state(&state);
  js_builder_destroy(jsb);

  return;
}

char* xs_get_js(CompileResult* result) {
  return result->js;
}

void destroy_xstate_result(CompileResult* result) {
  if(result->js != NULL) {
    free(result->js);
  }
  free(result);
}