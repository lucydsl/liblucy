#include "printer.h"

static void compile_local_node(Printer* printer, JSBuilder* jsb, LocalNode* local_node) {
  switch(local_node->key) {
    case LOCAL_ENTRY: {
      printer->in_entry = true;
      printer_compile_transition_action(printer, jsb, local_node->action, "entry");
      printer->in_entry = false;
      break;
    }
    case LOCAL_EXIT: {
      printer_compile_transition_action(printer, jsb, local_node->action, "exit");
      break;
    }
    default: {
      break;
    }
  }
}

void printer_enter_state(Printer* printer, JSBuilder* jsb, Node* node) {
  StateNode* state_node = (StateNode*)node;
  MachineNode* machine_node = (MachineNode*)node->parent;
  if(!(machine_node->printer_flags & MACHINE_PRINTED_STATES)) {
    machine_node->printer_flags |= MACHINE_PRINTED_STATES;
    js_builder_start_prop(jsb, "states");
    js_builder_start_object(jsb);
  }

  js_builder_start_prop_no_copy(jsb, printer->source, state_node->name_start, state_node->name_end);
  js_builder_start_object(jsb);

  if(state_node->final) {
    js_builder_start_prop(jsb, "type");
    js_builder_add_string(jsb, "final");
  }

  if(state_node->entry != NULL) {
    printer->entry_state_start = state_node->name_start;
    printer->entry_state_end = state_node->name_end;
    compile_local_node(printer, jsb, state_node->entry);
    printer->entry_state_start = 0;
    printer->entry_state_end = 0;
  }
  if(state_node->exit != NULL) {
    compile_local_node(printer, jsb, state_node->exit);
  }
  if(state_node->event_transition != NULL) {
    js_builder_start_prop(jsb, "on");
    js_builder_start_object(jsb);
    TransitionNode* transition_node = state_node->event_transition;
    while(transition_node != NULL) {
      printer_compile_event_transition(printer, jsb, transition_node);
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
      printer_compile_inner_transition(printer, jsb, transition_node);
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
      printer_compile_transition_key(printer, jsb, transition_node_node, NULL);
      printer_compile_inner_transition(printer, jsb, transition_node);
      transition_node = transition_node->next;
    } while(transition_node != NULL);
    js_builder_end_object(jsb);
  }
  if(state_node->invoke != NULL) {
    printer_compile_invoke(printer, jsb, state_node->invoke);
  }
}

void printer_exit_state(Printer* printer, JSBuilder* jsb, Node* node) {
  StateNode* state_node = (StateNode*)node;

  // Close out this state node.
  js_builder_end_object(jsb);

  // End of all state nodes
  if(!node->next || node->next->type != NODE_STATE) {
    js_builder_end_object(jsb);
  }
  str_set_clear(&printer->event_names);
}