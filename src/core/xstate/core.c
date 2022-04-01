#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../dict.h"
#include "../js_builder.h"
#include "../node.h"
#include "../set.h"
#include "core.h"
#include "machine.h"

typedef void (*set_ref)(PrintState*, Ref*);

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

void xs_destroy_state_refs(PrintState *state) {
  if(state->guard != NULL) {
    destroy_ref(state->guard);
    state->guard = NULL;
  }
  if(state->action != NULL) {
    destroy_ref(state->action);
    state->action = NULL;
  }
  if(state->delay != NULL) {
    destroy_ref(state->delay);
    state->delay = NULL;
  }
}

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

void xs_add_action_ref(PrintState* state, char* key, Expression* value) {
  add_ref(state, key, value, state->action, state->action_names, *set_action_ref);
}

static void set_guard_ref(PrintState* state, Ref* ref) {
  state->guard = ref;
}

void xs_add_guard_ref(PrintState* state, char* key, Expression* value) {
  add_ref(state, key, value, state->guard, state->guard_names, *set_guard_ref);
}

static void set_delay_ref(PrintState* state, Ref* ref) {
  state->delay = ref;
}

void xs_add_delay_ref(PrintState* state, char* key, Expression* value) {
  add_ref(state, key, value, state->delay, state->delay_names, *set_delay_ref);
}

static void set_service_ref(PrintState* state, Ref* ref) {
  state->service = ref;
}

void xs_add_service_ref(PrintState* state, char* key, Expression* value) {
  add_ref(state, key, value, state->service, state->service_names, *set_service_ref);
}

void xs_start_assign_call(JSBuilder* jsb, AssignExpression* assign_expression) {
  js_builder_start_call(jsb, "assign");
  js_builder_start_object(jsb);
  js_builder_start_prop(jsb, assign_expression->key);
}

void xs_end_assign_call(JSBuilder* jsb) {
  js_builder_end_object(jsb);
  js_builder_end_call(jsb);
}

char* xs_copy_str_from_source(PrintState* state, size_t start, size_t end) {
  size_t len = end - start;
  char* out = malloc(len + 1);
  size_t i = start;
  while(i < end) {
    out[i - start] = state->source[i];
    i++;
  }
  out[i] = '\0';
  return out;
}

bool xs_find_and_add_top_level_machine_name(PrintState* state, JSBuilder* jsb, char* matching_name) {
  unsigned long searchid = hash_function(matching_name);
  Node* cur = state->program->body;
  while(cur != NULL) {
    if(cur->type == NODE_MACHINE_TYPE) {
      MachineNode* cur_machine = (MachineNode*)cur;
      if(cur_machine->name_start != 0) {
        char* machine_name = xs_copy_str_from_source(state, cur_machine->name_start, cur_machine->name_end);
        if(hash_function(machine_name) == searchid) {
          xs_add_machine_binding_name(state, jsb, cur_machine);
          free(machine_name);
          return true;
        }
        free(machine_name);
      }
    }
    cur = cur->next;
  }
  return false;
}

void xs_add_spawn_call(PrintState* state, JSBuilder* jsb, SpawnExpression* spawn_expression) {
  js_builder_add_str(jsb, "() => ");
  js_builder_start_call(jsb, "spawn");
  Expression* target = spawn_expression->target;
  char* name = NULL;
  if(target->type == EXPRESSION_IDENTIFIER) {
    name = ((IdentifierExpression*)target)->name;
    if(!xs_find_and_add_top_level_machine_name(state, jsb, name)) {
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

void xs_add_send_call(JSBuilder* jsb, SendExpression* send_expression) {
  js_builder_start_call(jsb, "send");
  js_builder_add_str(jsb, "(ctx, ev) => ({ type: ");
  js_builder_add_string(jsb, send_expression->event);
  js_builder_add_str(jsb, ", data: ev.data })");
  js_builder_add_str(jsb, ", ");
  js_builder_start_object(jsb);
  js_builder_start_prop(jsb, "to");
  js_builder_add_str(jsb, "(context) => context.");
  js_builder_add_str(jsb, send_expression->actor);
  js_builder_end_object(jsb);
  js_builder_end_call(jsb);
}