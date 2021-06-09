#include "../js_builder.h"
#include "../node.h"
#include "core.h"

void xs_enter_assignment(PrintState* state, JSBuilder* jsb, Node* node) {
  Assignment* assignment = (Assignment*)node;

  switch(assignment->binding_type) {
    case ASSIGNMENT_ACTION: {
      xs_add_action_ref(state, assignment->binding_name, assignment->value);
      break;
    }
    case ASSIGNMENT_GUARD: {
      xs_add_guard_ref(state, assignment->binding_name, assignment->value);
      break;
    }
  }
}