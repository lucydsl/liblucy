#include "js.h"

#include <stdbool.h>
#include <stddef.h>

#include "js_builder.h"
#include "printer.h"

JSOutput js_print(Arena* arena, Program* program, const char* source,
  bool include_dts, DiagnosticList* diagnostics) {
  Printer printer = {
    .arena = arena,
    .diagnostics = diagnostics,
    .source = source,
    .program = program,
    .jsb = js_builder_create(),
  };
  printer_build_model(&printer);

  bool printed = false;
  for(Node* node = program->body; node != NULL; node = node->next) {
    if(node->type == NODE_IMPORT) {
      if(printer_print_import(&printer, (ImportNode*)node)) {
        printed = true;
      }
    }
  }
  for(JSMachine* machine = printer.machines; machine != NULL; machine = machine->next) {
    if(printed) {
      js_builder_blank(printer.jsb);
    }
    printer_print_machine(&printer, machine);
    printed = true;
  }

  JSOutput output = {
    .js = js_builder_release(printer.jsb),
    .dts = NULL
  };
  printer.jsb = NULL;
  if(include_dts) {
    output.dts = printer_print_dts(&printer);
  }
  return output;
}
