#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "../dict.h"
#include "../node.h"
#include "../program.h"
#include "../parser/parser.h"
#include "../str_builder.h"
#include "../js_builder.h"
#include "../local.h"
#include "../set.h"

#include "assignment.h"
#include "compiler.h"
#include "core.h"
#include "import.h"
#include "machine.h"
#include "state.h"

static void destroy_state(PrintState *state) {
  xs_destroy_state_refs(state);
  set_destroy(state->events);
  set_destroy(state->action_names);
  set_destroy(state->guard_names);
  set_destroy(state->delay_names);
  set_destroy(state->service_names);
}

CompileResult* xs_create() {
  CompileResult* result = malloc(sizeof(*result));
  return result;
}

void xs_init(CompileResult* result, int use_remote_source) {
  result->success = false;
  result->js = NULL;
  result->flags = 0;
  
  if(use_remote_source) {
    result->flags |= FLAG_USE_REMOTE;
  }
}

void compile_xstate(CompileResult* result, char* source, char* filename) {
  ParseResult *parse_result = parse(source, filename);

  if(parse_result->success == false) {
    result->success = false;
    result->js = NULL;
    return;
  }

  Program *program = parse_result->program;
  char* xstate_specifier;
  if(result->flags & FLAG_USE_REMOTE) {
    xstate_specifier = "https://cdn.skypack.dev/xstate";
  } else {
    xstate_specifier = "xstate";
  }

  JSBuilder *jsb;
  Node* node;
  jsb = js_builder_create();
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
  }

  PrintState state = {
    .program = program,
    .guard = NULL,
    .action = NULL,
    .delay = NULL,
    .events = malloc(sizeof(SimpleSet)),
    .guard_names = malloc(sizeof(SimpleSet)),
    .action_names = malloc(sizeof(SimpleSet)),
    .delay_names = malloc(sizeof(SimpleSet)),
    .service_names = malloc(sizeof(SimpleSet))
  };
  set_init(state.events);
  set_init(state.guard_names);
  set_init(state.action_names);
  set_init(state.delay_names);
  set_init(state.service_names);

  bool exit = false;
  while(node != NULL) {
    unsigned short type = node->type;

    if(exit) {
      switch(type) {
        case NODE_STATE_TYPE: {
          xs_exit_state(&state, jsb, node);
          node_destroy_state((StateNode*)node);
          break;
        }
        case NODE_TRANSITION_TYPE: {
          node_destroy_transition((TransitionNode*)node);
          break;
        }
        case NODE_ASSIGNMENT_TYPE: {
          node_destroy_assignment((Assignment*)node);
          break;
        }
        case NODE_IMPORT_SPECIFIER_TYPE: {
          node_destroy_import_specifier((ImportSpecifier*)node);
          break;
        }
        case NODE_IMPORT_TYPE: {
          node_destroy_import((ImportNode*)node);
          break;
        }
        case NODE_MACHINE_TYPE: {
          xs_exit_machine(&state, jsb, node);
          node_destroy_machine((MachineNode*)node);
          break;
        }
        case NODE_INVOKE_TYPE: {
          node_destroy_invoke((InvokeNode*)node);
          break;
        }
        default: {
          printf("Node type %hu not torn down (this is a compiler bug)\n", type);
          break;
        }
      }
    } else {
      switch(type) {
        case NODE_IMPORT_TYPE: {
          xs_enter_import(&state, jsb, node);
          break;
        }
        case NODE_STATE_TYPE: {
          xs_enter_state(&state, jsb, node);
          break;
        }
        case NODE_TRANSITION_TYPE: {
          break;
        }
        case NODE_ASSIGNMENT_TYPE: {
          xs_enter_assignment(&state, jsb, node);
          break;
        }
        case NODE_INVOKE_TYPE: {
          break;
        }
        case NODE_LOCAL_TYPE: {
          // Do not walk down local nodes.
          exit = true;
          node_destroy_local((LocalNode*)node);
          break;
        }
        case NODE_MACHINE_TYPE: {
          xs_enter_machine(&state, jsb, node);
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
      node_destroy(node);
      node = node->parent;
    } else {
      // Reached the end.
      node_destroy(node);
      break;
    }
  }

  char* js = js_builder_dump(jsb);
  result->success = true;
  result->js = js;

  // Teardown
  destroy_state(&state);
  js_builder_destroy(jsb);

  return;
}

char* xs_get_js(CompileResult* result) {
  return result->js;
}

void destroy_xstate_result(CompileResult* result) {
  if(result->js != NULL) {
    free(result->js);
  }
  free(result);
}