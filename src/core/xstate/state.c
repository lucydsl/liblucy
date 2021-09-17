#include "../js_builder.h"
#include "../local.h"
#include "../node.h"
#include "../set.h"
#include "core.h"
#include "invoke.h"
#include "transition.h"
#include "ts_printer.h"

static void compile_local_node(PrintState* state, JSBuilder* jsb, LocalNode* local_node) {
  switch(local_node->key) {
    case LOCAL_ENTRY: {
      state->in_entry = true;
      xs_compile_transition_action(state, jsb, local_node->action, "entry");
      state->in_entry = false;
      break;
    }
    case LOCAL_EXIT: {
      xs_compile_transition_action(state, jsb, local_node->action, "exit");
      break;
    }
  }
}

void xs_enter_state(PrintState* state, JSBuilder* jsb, Node* node) {
  StateNode* state_node = (StateNode*)node;
  MachineNode* machine_node = (MachineNode*)node->parent;
  if(!(machine_node->impl_flags & XS_HAS_STATE_PROP)) {
    machine_node->impl_flags |= XS_HAS_STATE_PROP;
    js_builder_start_prop(jsb, "states");
    js_builder_start_object(jsb);
  }

  js_builder_start_prop(jsb, state_node->name);
  js_builder_start_object(jsb);

  if(state->flags & XS_FLAG_DTS) {
    ts_printer_add_event(state->tsprinter, state_node->name);
  }

  if(state_node->final) {
    js_builder_start_prop(jsb, "type");
    js_builder_add_string(jsb, "final");
  }

  if(state_node->entry != NULL) {
    state->cur_state_name = state_node->name;
    compile_local_node(state, jsb, state_node->entry);
    state->cur_state_name = NULL;
  }
  if(state_node->exit != NULL) {
    compile_local_node(state, jsb, state_node->exit);
  }
  if(state_node->event_transition != NULL) {
    js_builder_start_prop(jsb, "on");
    js_builder_start_object(jsb);
    TransitionNode* transition_node = state_node->event_transition;
    while(transition_node != NULL) {
      xs_compile_event_transition(state, jsb, transition_node);
      transition_node = transition_node->next;
    }
    js_builder_end_object(jsb);
  }
  if(state_node->immediate_transition != NULL) {
    js_builder_start_prop(jsb, "always");
    js_builder_start_array(jsb, true);
    js_builder_add_indent(jsb);

    TransitionNode* transition_node = state_node->immediate_transition;
    bool comma = false;
    do {
      if(comma) {
        js_builder_add_str(jsb, ", ");
      }
      xs_compile_inner_transition(state, jsb, transition_node);
      transition_node = transition_node->next;
      comma = true;
    } while(transition_node != NULL);
    js_builder_end_array(jsb, true);
  }
  if(state_node->delay_transition != NULL) {
    js_builder_start_prop(jsb, "after");
    js_builder_start_object(jsb);
    TransitionNode* transition_node = state_node->delay_transition;
    do {
      Node* transition_node_node = (Node*)transition_node;
      xs_compile_transition_key(state, jsb, transition_node_node, NULL);
      xs_compile_inner_transition(state, jsb, transition_node);
      transition_node = transition_node->next;
    } while(transition_node != NULL);
    js_builder_end_object(jsb);
  }
  if(state_node->invoke != NULL) {
    xs_compile_invoke(state, jsb, state_node->invoke);
  }
}

void xs_exit_state(PrintState* state, JSBuilder* jsb, Node* node) {
  StateNode* state_node = (StateNode*)node;

  // Close out this state node.
  js_builder_end_object(jsb);

  // End of all state nodes
  if(!node->next || node->next->type != NODE_STATE_TYPE) {
    js_builder_end_object(jsb);
  }
  set_clear(state->events);
}