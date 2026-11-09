#include "xstate.h"

#include <stdbool.h>
#include <stddef.h>

#include "js_builder.h"
#include "printer.h"
#include "ts_printer.h"

XStateOutput xstate_print(Arena* arena, Program* program, char* source,
  bool use_remote_imports, bool include_dts, DiagnosticList* diagnostics) {
  char* xstate_specifier;
  if(use_remote_imports) {
    xstate_specifier = "https://cdn.skypack.dev/xstate";
  } else {
    xstate_specifier = "xstate";
  }

  JSBuilder *jsb;
  jsb = js_builder_create();

  Printer printer = {
    .arena = arena,
    .diagnostics = diagnostics,
    .source = source,
    .program = program,
    .guard_refs = NULL,
    .action_refs = NULL,
    .delay_refs = NULL,
    .service_refs = NULL,
    .ts_printer = NULL,
    .current_event = NULL,
    .entry_state_start = 0,
    .entry_state_end = 0,
    .in_entry = false
  };
  str_set_init(&printer.event_names, arena);
  str_set_init(&printer.guard_names, arena);
  str_set_init(&printer.action_names, arena);
  str_set_init(&printer.delay_names, arena);
  str_set_init(&printer.service_names, arena);

  if(include_dts) {
    printer.ts_printer = ts_printer_create(arena, source);
  }

  Node* node;
  node = program->body;

  if(node != NULL) {
    js_builder_add_str(jsb, "import { createMachine");

    if(program->flags & PROGRAM_USES_ASSIGN) {
      js_builder_add_str(jsb, ", assign");
    }
    if(program->flags & PROGRAM_USES_SEND) {
      js_builder_add_str(jsb, ", send");
    }
    if(program->flags & PROGRAM_USES_SPAWN) {
      js_builder_add_str(jsb, ", spawn");
    }

    js_builder_add_str(jsb, " } from '");

    js_builder_add_str(jsb, xstate_specifier);
    js_builder_add_str(jsb, "';\n");
    if(include_dts) {
      printer.ts_printer->xstate_specifier = xstate_specifier;
    }
  }

  // Walk the tree depth first, calling enter on the way down and exit on
  // the way back up. The nodes belong to the arena, so nothing is freed.
  bool exit = false;
  while(node != NULL) {
    NodeType type = node->type;

    if(exit) {
      switch(type) {
        case NODE_STATE: {
          printer_exit_state(&printer, jsb, node);
          break;
        }
        case NODE_MACHINE: {
          printer_exit_machine(&printer, jsb, node);
          break;
        }
        default: {
          break;
        }
      }
    } else {
      switch(type) {
        case NODE_IMPORT: {
          printer_enter_import(&printer, jsb, node);
          break;
        }
        case NODE_STATE: {
          printer_enter_state(&printer, jsb, node);
          break;
        }
        case NODE_TRANSITION: {
          break;
        }
        case NODE_ASSIGNMENT: {
          printer_enter_assignment(&printer, node);
          break;
        }
        case NODE_INVOKE: {
          break;
        }
        case NODE_LOCAL: {
          // Do not walk down local nodes.
          exit = true;
          break;
        }
        case NODE_MACHINE: {
          printer_enter_machine(&printer, jsb, node);
          break;
        }
        default: {
          break;
        }
      }
    }

    // Node has no children, so go again for the exit.
    if(!exit && !node->child) {
      exit = true;
      continue;
    }
    // Node has a child
    else if(!exit && node->child) {
      node = node->child;
    } else if(node->next) {
      exit = false;
      node = node->next;
    } else if(node->parent) {
      exit = true;
      node = node->parent;
    } else {
      // Reached the end.
      break;
    }
  }

  XStateOutput output = {
    .js = js_builder_release(jsb),
    .dts = NULL
  };

  if(include_dts) {
    output.dts = ts_printer_dump(printer.ts_printer);
    ts_printer_destroy(printer.ts_printer);
  }

  return output;
}
