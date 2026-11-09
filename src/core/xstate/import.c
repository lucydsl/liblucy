#include "printer.h"

#include <stdio.h>

void printer_enter_import(Printer* printer, JSBuilder* jsb, Node* node) {
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
    ImportSpecifierNode *specifier = (ImportSpecifierNode*)child;

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