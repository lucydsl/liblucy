#include "printer.h"

void printer_print_import(Printer* printer, ImportNode* import_node) {
  JSBuilder* jsb = printer->jsb;
  Node* child = import_node->node.child;
  if(child == NULL) {
    printer_warn(printer, LUCY_STREAM_STDOUT, "TODO add support for imports with no specifiers\n");
    return;
  }
  js_builder_add_str(jsb, "import { ");
  for(; child != NULL; child = child->next) {
    ImportSpecifierNode* specifier = (ImportSpecifierNode*)child;
    js_builder_add_str(jsb, specifier->imported);
    if(child->next != NULL) {
      js_builder_add_str(jsb, ", ");
    }
  }
  // The source keeps its quotes.
  js_builder_add_format(jsb, " } from %s;\n", import_node->from);
}
