#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "node.h"
#include "program.h"
#include "parser.h"
#include "str_builder.h"
#include "js_builder.h"
#include "compiler_xstate.h"

typedef struct Ref {
  char* key;
  struct Ref* next;
  Expression* value;
} Ref;

typedef struct PrintState {
  bool state_prop_added;
  bool on_prop_added;
  bool always_prop_added;
  Ref* guard;
  Ref* action;
} PrintState;

static void add_action_ref(PrintState* state, char* key, Expression* value) {
  Ref *ref = malloc(sizeof(Ref));
  ref->key = key;
  ref->value = node_clone_expression(value);
  ref->next = NULL;

  if(state->action == NULL) {
    state->action = ref;
  } else {
    Ref* cur = state->action;
    while(cur->next != NULL) {
      cur = cur->next;
    }
    cur->next = ref;
  }
}

static void add_guard_ref(PrintState* state, char* key, Expression* value) {
  Ref *ref = malloc(sizeof(Ref));
  ref->key = key;
  ref->value = node_clone_expression(value);
  ref->next = NULL;

  if(state->guard == NULL) {
    state->guard = ref;
  } else {
    Ref* cur = state->guard;
    while(cur->next != NULL) {
      cur = cur->next;
    }
    cur->next = ref;
  }
}

static void destroy_ref(Ref* ref) {
  if(ref->next != NULL) {
    destroy_ref(ref->next);
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
}

static void enter_machine(PrintState* state, JSBuilder* jsb, Node* node) {
  MachineNode *machine_node = (MachineNode*)node;
  Node* parent_node = node->parent;
  bool is_nested = node_machine_is_nested(node);

  if(!is_nested) {
    if(machine_node->name == NULL) {
      js_builder_add_str(jsb, "\nexport default ");
    } else {
      js_builder_add_str(jsb, "\nexport const ");
      js_builder_add_str(jsb, machine_node->name);
      js_builder_add_str(jsb, " = ");
    }
    js_builder_start_call(jsb, "Machine");
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

  if(is_nested) {
    js_builder_start_prop(jsb, "states");
    js_builder_start_object(jsb);
  }
}

static void exit_machine(PrintState* state, JSBuilder* jsb, Node* node) {
  bool has_guard = state->guard != NULL;
  bool has_action = state->action != NULL;
  bool needs_options = has_guard || has_action;
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

        if(expression->type != EXPRESSION_IDENTIFIER) {
          printf("Unexpected type of expression\n");
          break;
        }

        char* identifier = ((IdentifierExpression*)expression)->name;
        js_builder_add_str(jsb, identifier);

        ref = ref->next;
      }

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

            js_builder_start_call(jsb, "assign");
            js_builder_start_object(jsb);
            js_builder_start_prop(jsb, assign->key);
            js_builder_add_str(jsb, assign->identifier);
            js_builder_end_object(jsb);
            js_builder_end_call(jsb);

            //js_builder_add_string(jsb, assign->identifier);
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

    js_builder_end_object(jsb);
  } else if(!is_nested) {
    js_builder_end_object(jsb);
  }

  if(!is_nested) {
    js_builder_end_call(jsb);
    js_builder_add_str(jsb, ";");
  }

  state->state_prop_added = false;
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
  if(!state->state_prop_added) {
    state->state_prop_added = true;
    js_builder_start_prop(jsb, "states");
    js_builder_start_object(jsb);
  }

  StateNode* state_node = (StateNode*)node;

  js_builder_start_prop(jsb, state_node->name);
  js_builder_start_object(jsb);

  if(state_node->final) {
    js_builder_start_prop(jsb, "type");
    js_builder_add_string(jsb, "final");
  }
}

static void exit_state(PrintState* state, JSBuilder* jsb, Node* node) {
  js_builder_end_object(jsb);

  StateNode* state_node = (StateNode*)node;

  // End of all state nodes
  if(!node->next || node->next->type != NODE_STATE_TYPE) {
    js_builder_end_object(jsb);
  }
}

static void enter_invoke(PrintState* state, JSBuilder* jsb, Node* node) {
  js_builder_start_prop(jsb, "invoke");
  js_builder_start_object(jsb);

  InvokeNode* invoke_node = (InvokeNode*)node;
  js_builder_start_prop(jsb, "src");
  js_builder_add_str(jsb, invoke_node->call);
}

static void exit_invoke(PrintState* state, JSBuilder* jsb, Node* node) {
  js_builder_end_object(jsb);
}

static void enter_transition(PrintState* state, JSBuilder* jsb, Node* node) {
  TransitionNode* transition_node = (TransitionNode*)node;
  Node* parent_node = node->parent;
  char* event_name = transition_node->event;
  bool is_always = transition_node->always;

  if(parent_node->type == NODE_INVOKE_TYPE) {
    if(strcmp(event_name, "done") == 0) {
      js_builder_start_prop(jsb, "onDone");
    } else if(strcmp(event_name, "error") == 0) {
      js_builder_start_prop(jsb, "onError");
    } else {
      printf("Regular events in invoke are not supported.\n");
    }
  } else {
    if(is_always) {
      if(!state->always_prop_added) {
        state->always_prop_added = true;
        js_builder_start_prop(jsb, "always");
        js_builder_start_array(jsb, true);
        js_builder_add_indent(jsb);
      }
    } else {
      if(!state->on_prop_added) {
        state->on_prop_added = true;
        js_builder_start_prop(jsb, "on");
        js_builder_start_object(jsb);
      }

      js_builder_start_prop(jsb, event_name);
    }
  }

  bool has_guard = transition_node->guard != NULL;
  bool has_action = transition_node->action != NULL;
  bool has_guard_or_action = has_guard || has_action;
  bool use_object_notation = has_guard_or_action || is_always;

  if(use_object_notation) {
    js_builder_start_object(jsb);
    js_builder_start_prop(jsb, "target");
    js_builder_add_string(jsb, transition_node->dest);

    if(has_guard) {
      js_builder_start_prop(jsb, "cond");

      TransitionGuard* guard = transition_node->guard;
      // If there are multiple guards use an array.
      if(guard->next) {
        js_builder_start_array(jsb, false);
        while(true) {
          js_builder_add_string(jsb, guard->name);
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
        js_builder_add_string(jsb, guard->name);
      }
    }

    if(has_action) {
      js_builder_start_prop(jsb, "actions");
      js_builder_start_array(jsb, false);
      TransitionAction* action = transition_node->action;
      while(true) {
        js_builder_add_string(jsb, action->name);

        action = action->next;

        if(action == NULL) {
          break;
        }

        js_builder_add_str(jsb, ", ");
      }

      js_builder_end_array(jsb, false);
    }

    js_builder_end_object(jsb);

    if(is_always) {
      js_builder_end_array(jsb, true);
    }
  } else {
    js_builder_add_string(jsb, transition_node->dest);
  }
}

static void exit_transition(PrintState* state, JSBuilder* jsb, Node* node) {
  if(node->next) {
    
  } else if(state->on_prop_added) {
    state->on_prop_added = false;

    js_builder_end_object(jsb);
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

CompileResult* compile_xstate(char* source, char* filename) {
  ParseResult *parse_result = parse(source, filename);

  if(parse_result->success == false) {
    CompileResult *result = malloc(sizeof(*result));
    result->success = false;
    result->js = NULL;
    return result;
  }

  Program *program = parse_result->program;
  char* xstate_specifier = "xstate"; // TODO support remove "https://cdn.skypack.dev/xstate"

  JSBuilder *jsb;
  Node* node;
  jsb = js_builder_create();
  node = program->body;

  if(node != NULL)
  {
    js_builder_add_str(jsb, "import { Machine } from '");
    js_builder_add_str(jsb, xstate_specifier);
    js_builder_add_str(jsb, "';\n");
  }

  PrintState state;
  state.state_prop_added = false;
  state.on_prop_added = false;
  state.always_prop_added = false;
  state.guard = NULL;
  state.action = NULL;

  bool exit = false;
  while(node != NULL)
  {
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
  CompileResult* result = malloc(sizeof(*result));
  result->success = true;
  result->js = js;

  // Teardown
  destroy_state(&state);
  js_builder_destroy(jsb);

  return result;
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