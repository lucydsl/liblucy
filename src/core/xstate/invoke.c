#include "printer.h"

#include <stdbool.h>

void printer_compile_invoke(Printer* printer, JSBuilder* jsb, InvokeNode* invoke_node) {
  js_builder_start_prop(jsb, "invoke");
  js_builder_start_object(jsb);

  js_builder_start_prop(jsb, "src");

  InvokeExpression* invoke_expression = (InvokeExpression*)invoke_node->expr;
  switch(invoke_expression->ref->type) {
    case EXPRESSION_IDENTIFIER: {
      IdentifierExpression* identifier_expression = (IdentifierExpression*)invoke_expression->ref;

      // Looking for a machine matching this name.
      if(!printer_add_top_level_machine_name(printer, jsb, identifier_expression->name)) {
        // No in-scope machine found, so just add the identifier directly.
        js_builder_add_str(jsb, identifier_expression->name);
      }
      break;
    }
    case EXPRESSION_SYMBOL: {
      SymbolExpression* symbol_expression = (SymbolExpression*)invoke_expression->ref;
      js_builder_add_string(jsb, symbol_expression->name);
      printer_add_service_ref(printer, symbol_expression->name, (Expression*)invoke_expression);

      if(printer->ts_printer != NULL) {
        ts_printer_add_invoke(printer->ts_printer, symbol_expression->name);
      }
      break;
    }
    default: {
      break;
    }
  }

  if(invoke_node->event_transition != NULL) {
    TransitionNode* transition_node = invoke_node->event_transition;
    do {
      printer_compile_event_transition(printer, jsb, transition_node);
      transition_node = transition_node->next;
    } while(transition_node != NULL);
  }

  js_builder_end_object(jsb);
}
