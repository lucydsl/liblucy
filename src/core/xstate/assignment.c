#include "printer.h"

void printer_enter_assignment(Printer* printer, JSBuilder* jsb, Node* node) {
  AssignmentNode* assignment = (AssignmentNode*)node;

  switch(assignment->binding_type) {
    case ASSIGNMENT_ACTION: {
      printer_add_action_ref(printer, assignment->binding_name, assignment->value);
      break;
    }
    case ASSIGNMENT_GUARD: {
      printer_add_guard_ref(printer, assignment->binding_name, assignment->value);
      break;
    }
  }
}