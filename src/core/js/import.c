#include "printer.h"

bool printer_print_import(Printer* printer, ImportNode* import_node) {
  JSBuilder* jsb = printer->jsb;
  if(import_node->node.child == NULL) {
    printer_warn(printer, LUCY_STREAM_STDOUT, "TODO add support for imports with no specifiers\n");
    return false;
  }
  // The names used as the types of events are types, which only the .d.ts
  // imports.
  bool any = false;
  for(Node* child = import_node->node.child; child != NULL; child = child->next) {
    ImportSpecifierNode* specifier = (ImportSpecifierNode*)child;
    if(printer_is_type_name(printer, specifier->imported)) {
      continue;
    }
    js_builder_add_str(jsb, any ? ", " : "import { ");
    js_builder_add_str(jsb, specifier->imported);
    any = true;
  }
  if(any) {
    // The source keeps its quotes.
    js_builder_add_format(jsb, " } from %s;\n", import_node->from);
  }
  return any;
}
