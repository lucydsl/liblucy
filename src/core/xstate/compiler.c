#include "compiler.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../ast/program.h"
#include "../parser/parser.h"
#include "../support/arena.h"
#include "../support/mem.h"
#include "printer.h"
#include "ts_printer.h"

// Free the strings a compile left in result, so the result can be used
// again.
static void clear_output(CompileResult* result) {
  free(result->js);
  free(result->dts);
  result->success = false;
  result->js = NULL;
  result->dts = NULL;
}

CompileResult* xs_create(void) {
  // Zeroed, so xs_init and compile_xstate can tell there is no output yet.
  // malloc left the struct uninitialised until xs_init, and wasn't checked.
  return xcalloc(1, sizeof(CompileResult));
}

void xs_init(CompileResult* result, bool use_remote_source, bool include_dts) {
  // A result may be initialised again and reused; set js and dts to NULL
  // without freeing them leaked the last compile's output.
  clear_output(result);
  result->flags = 0;
  
  if(use_remote_source) {
    result->flags |= XS_FLAG_USE_REMOTE;
  }
  if(include_dts) {
    result->flags |= XS_FLAG_DTS;
  }
}

void compile_xstate(CompileResult* result, char* source, char* filename) {
  // Output from an earlier compile into the same result is replaced.
  clear_output(result);

  if(source == NULL || filename == NULL) {
    return;
  }

  // Everything this compile allocates, apart from the strings it returns
  // in result, comes from this arena and is freed with it at the end.
  Arena arena;
  arena_init(&arena);

  ParseResult parse_result = parse(&arena, source, filename);

  if(parse_result.success == false) {
    arena_destroy(&arena);
    return;
  }

  Program *program = parse_result.program;
  char* xstate_specifier;
  if(result->flags & XS_FLAG_USE_REMOTE) {
    xstate_specifier = "https://cdn.skypack.dev/xstate";
  } else {
    xstate_specifier = "xstate";
  }

  JSBuilder *jsb;
  jsb = js_builder_create();

  Printer printer = {
    .flags = result->flags,
    .arena = &arena,
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
  str_set_init(&printer.event_names, &arena);
  str_set_init(&printer.guard_names, &arena);
  str_set_init(&printer.action_names, &arena);
  str_set_init(&printer.delay_names, &arena);
  str_set_init(&printer.service_names, &arena);

  if(printer.flags & XS_FLAG_DTS) {
    printer.ts_printer = ts_printer_create(&arena, source);
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
    if(result->flags & XS_FLAG_DTS) {
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
          printer_enter_assignment(&printer, jsb, node);
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

  result->success = true;
  result->js = js_builder_release(jsb);

  if(result->flags & XS_FLAG_DTS) {
    char* dts = ts_printer_dump(printer.ts_printer);
    result->dts = dts;
    ts_printer_destroy(printer.ts_printer);
  }

  // Teardown
  arena_destroy(&arena);
}

char* xs_get_js(CompileResult* result) {
  return result->js;
}

char* xs_get_dts(CompileResult* result) {
  return result->dts;
}

void destroy_xstate_result(CompileResult* result) {
  if(result == NULL) {
    return;
  }
  clear_output(result);
  free(result);
}