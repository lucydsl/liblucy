#include <stdio.h>
#include <string.h>
#include "node.h"
#include "program.h"
#include "parser.h"
#include "str_builder.h"
#include "js_builder.h"

typedef struct PrintState {
  bool state_prop_added;
  bool on_prop_added;
  bool machine_call_added;
} PrintState;

static void enter_import(PrintState* state, JSBuilder* jsb, Node* node) {
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
    ImportSpecifier *specifier = (ImportSpecifier*)child;

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

static void enter_state(PrintState* state, JSBuilder* jsb, Node* node) {
  if(!state->machine_call_added) {
    state->machine_call_added = true;
    js_builder_add_str(jsb, "\nexport default Machine({\n");
  }
  if(!state->state_prop_added) {
    state->state_prop_added = true;
    js_builder_add_indent(jsb);
    js_builder_add_str(jsb, "states: {");
    
    // Indent
    js_builder_increase_indent(jsb);
  } else {
    js_builder_add_str(jsb, ",");
  }

  js_builder_add_str(jsb, "\n");
  js_builder_add_indent(jsb);
  js_builder_add_str(jsb, ((StateNode*)node)->name);
  js_builder_add_str(jsb, ": {");
}

static void exit_state(PrintState* state, JSBuilder* jsb, Node* node) {
  js_builder_add_str(jsb, "\n");
  js_builder_decrease_indent(jsb);
  js_builder_add_indent(jsb);
  js_builder_add_str(jsb, "}");

  // End of all state nodes
  if(!node->next || node->next->type != NODE_STATE_TYPE) {
    js_builder_add_str(jsb, "\n");
    js_builder_decrease_indent(jsb);
    js_builder_add_indent(jsb);
    js_builder_add_str(jsb, "}\n");
  }
}

static void enter_transition(PrintState* state, JSBuilder* jsb, Node* node) {
  if(!state->on_prop_added) {
    state->on_prop_added = true;
    js_builder_add_str(jsb, "\n");
    js_builder_increase_indent(jsb);
    js_builder_add_indent(jsb);
    js_builder_add_str(jsb, "on: {");
    js_builder_increase_indent(jsb);
  }

  TransitionNode* transition_node = (TransitionNode*)node;
  js_builder_add_str(jsb, "\n");
  js_builder_add_indent(jsb);
  js_builder_safe_key(jsb, transition_node->event);
  js_builder_add_str(jsb, ": ");
  js_builder_add_string(jsb, transition_node->dest);
}

static void exit_transition(PrintState* state, JSBuilder* jsb, Node* node) {
  state->on_prop_added = false;

  if(node->next) {
    js_builder_add_str(jsb, ",");
  } else {
    js_builder_add_str(jsb, "\n");
    js_builder_decrease_indent(jsb);
    js_builder_add_indent(jsb);
    js_builder_add_str(jsb, "}");
  }
}

char* compile_xstate(char * source) {
  Program * program = parse(source);
  char* xstate_specifier = "https://cdn.skypack.dev/xstate";// "xstate";

  JSBuilder *jsb;
  Node* node;
  jsb = js_builder_create();
  node = program->body;

  if(node != NULL)
  {
    js_builder_add_str(jsb, "import { Machine } from '");
    js_builder_add_str(jsb, xstate_specifier);
    js_builder_add_str(jsb, "';\n");
  }

  struct PrintState state;
  state.state_prop_added = false;
  state.on_prop_added = false;
  state.machine_call_added = false;

  bool exit = false;
  while(node != NULL)
  {
    unsigned short type = node->type;

    if(exit) {
      switch(type) {
        case NODE_STATE_TYPE: {
          exit_state(&state, jsb, node);
          break;
        }
        case NODE_TRANSITION_TYPE: {
          exit_transition(&state, jsb, node);
          break;
        }
      }
    } else {
      switch(type) {
        case NODE_IMPORT_TYPE: {
          enter_import(&state, jsb, node);
          break;
        }
        case NODE_STATE_TYPE: {
          enter_state(&state, jsb, node);
          break;
        }
        case NODE_TRANSITION_TYPE: {
          enter_transition(&state, jsb, node);
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

  js_builder_add_str(jsb, "});");

  char* out = js_builder_dump(jsb);

  return out;
}