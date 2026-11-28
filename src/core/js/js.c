#include "js.h"

#include <stdbool.h>
#include <stddef.h>

#include "js_builder.h"
#include "printer.h"

JSOutput js_print(Arena* arena, Program* program, const char* source,
  JSFormat format, JSTarget target, bool hydrate, DiagnosticList* diagnostics) {
  Printer printer = {
    .arena = arena,
    .diagnostics = diagnostics,
    .source = source,
    .program = program,
    .jsb = js_builder_create(),
    .ts = format == JS_FORMAT_TS,
    .ssr = target == JS_TARGET_SSR,
    .hydrate = hydrate,
  };
  printer_build_model(&printer);

  if(printer.ts) {
    // The module's types are those of its declarations, which TypeScript
    // checks wherever it is used. Inside the factories the events and the
    // machine are `any`, and a state nothing goes to leaves a function
    // unused, which a project's own settings (noUnusedLocals) could make an
    // error in code its author can't change.
    js_builder_line(printer.jsb, "// Compiled from Lucy by lucyc.");
    js_builder_line(printer.jsb, "// @ts-nocheck");
    js_builder_blank(printer.jsb);
  }

  bool printed = false;
  for(Node* node = program->body; node != NULL; node = node->next) {
    if(node->type == NODE_IMPORT) {
      if(printer_print_import(&printer, (ImportNode*)node)) {
        printed = true;
      }
    }
  }
  if(printer.ts && printer_print_type_imports(&printer, printer.jsb)) {
    printed = true;
  }
  bool helper = false;
  for(JSMachine* machine = printer.machines; machine != NULL; machine = machine->next) {
    if(printed) {
      js_builder_blank(printer.jsb);
    }
    if(machine->view != NULL && !helper) {
      printer_print_view_helper(&printer);
      helper = true;
    }
    printer_print_view_templates(&printer, machine);
    printer_print_machine(&printer, machine);
    printed = true;
  }

  JSOutput output = { 0 };
  char* text = js_builder_release(printer.jsb);
  printer.jsb = NULL;
  if(printer.ts) {
    output.ts = text;
  } else {
    output.js = text;
    output.dts = printer_print_dts(&printer);
  }
  return output;
}
