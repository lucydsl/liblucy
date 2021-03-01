#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "node.h"
#include "state.h"
#include "scope.h"
#include "identifier.h"
#include "program.h"
#include "str_builder.h"
#include "keyword.h"
#include "parser.h"
#include "error.h"

#define TOKEN_EOF 0
#define TOKEN_EOL 1
#define TOKEN_IDENTIFIER 2
#define TOKEN_ASSIGNMENT 3
#define TOKEN_CALL 4
#define TOKEN_BEGIN_BLOCK 5
#define TOKEN_END_BLOCK 6
#define TOKEN_STRING 7
#define TOKEN_UNKNOWN 8

#define _check(f) { int _fa = f; if(_fa == 2)  { return 2; } else if(_fa > err) { err = _fa; } }

static int consume_machine(State*);

int is_newline(char c) {
  return c == '\n';
}

int is_whitespace(char c) {
  return c == ' ';
}

static char* consume_string(State* state) {
  char c = state_char(state);
  char quote = c;
  bool in_quote = true;

  str_builder_t *sb;
  sb = str_builder_create();

  do {
    str_builder_add_char(sb, c);

    state_advance_column(state);
    state_next(state);


    c = state_char(state);
  } while(c != quote && c != '\n');

  str_builder_add_char(sb, quote);

  char* str = str_builder_dump(sb, NULL);
  state_set_word(state, str);
  
  return str;
}

static char* consume_identifier(State* state) {
  char c = state_char(state);
  str_builder_t *sb;
  sb = str_builder_create();

  do {
    str_builder_add_char(sb, c);

    state_advance_column(state);
    state_next(state);
    c = state_char(state);
  } while(state_inbounds(state) && is_valid_identifier_char(c));
  state_prev(state);

  char* str = str_builder_dump(sb, NULL);
  state_set_word(state, str);

  return str;
}

static int consume_token(State* state) {
  while(state_inbounds(state)) {
    state_next(state);
    state_advance_column(state);
    char c = state_char(state);

    if(is_whitespace(c)) {
      continue;
    }

    if(is_newline(c)) {
      state_advance_line(state);
      return TOKEN_EOL;
    }

    if(c == '{') {
      return TOKEN_BEGIN_BLOCK;
    }

    if(c == '}') {
      return TOKEN_END_BLOCK;
    }

    if(c == '=') {
      if(state_peek(state) == '>') {
        state_next(state);
        return TOKEN_CALL;
      }
      return TOKEN_ASSIGNMENT;
    }

    if(is_valid_identifier_char(c)) {
      consume_identifier(state);
      return TOKEN_IDENTIFIER;
    }

    if(c == '\'' || c == '"') {
      consume_string(state);
      return TOKEN_STRING;
    }

    if(c == '\0') {
      return TOKEN_EOF;
    }

    return TOKEN_UNKNOWN;
  }

  return TOKEN_EOF;
}

static int consume_transition(State* state) {
  int err = 0;
  char* event = state_take_word(state);

  TransitionNode* transition_node = node_create_transition();
  transition_node->event = event;

  // Always transition
  if(event == NULL) {
    transition_node->always = event == NULL;

    // Currently in a call, so rewind to back out.
    state_prev(state);
    state_prev(state);
  }

  // Parent should be a state node, should we check here? TODO
  Node* current_node = state->node;
  size_t current_node_type = current_node->type;

  Node* transition_node_node = (Node*)transition_node;
  int rewind_to_start = event == NULL ? 2 : strlen(event);
  state_node_start_pos(state, transition_node_node, rewind_to_start);

  switch(current_node_type) {
    case NODE_STATE_TYPE: {
      node_append(current_node, transition_node_node);
      state->parent_node = current_node;
      break;
    }
    case NODE_INVOKE_TYPE: {
      node_append(current_node, transition_node_node);
      state->parent_node = current_node;
      break;
    }
    case NODE_TRANSITION_TYPE: {
      error_msg_with_code_block(state, transition_node_node, "A transition sibiling to another, hasn't happened before, this is likely a compiler bug.");
      return 2;
    }
    default: {
      error_msg_with_code_block(state, transition_node_node, "Unexpected parent node to a transition.");
      return 2;
    }
  }

  StateNode* state_node = (StateNode*)current_node;

  state->node = transition_node_node;

  bool block_transition = false;
  char* identifier = NULL;
  while(true) {
    int token = consume_token(state);

    if(token == TOKEN_EOL) {
      if(block_transition) {
        token = consume_token(state);

        while(token == TOKEN_EOL) {
          token = consume_token(state);
        }

        if(token != TOKEN_END_BLOCK) {
          error_msg_with_code_block(state, transition_node_node, "Block transition expects destination state.");
          err = 2;
          goto end;
        }

        break;
      }
      // End of the transition
      break;
    }

    if(token != TOKEN_CALL) {
      error_msg_with_code_block(state, NULL, "Expected to pipe to a destination.");
      err = 2;
      goto end;
    }

    token = consume_token(state);

    while(block_transition && token == TOKEN_EOL) {
      token = consume_token(state);
    }

    if(token == TOKEN_BEGIN_BLOCK && !block_transition) {
      block_transition = true;
      token = consume_token(state);

      while(token == TOKEN_EOL) {
        token = consume_token(state);
      }
    }

    if(token != TOKEN_IDENTIFIER) {
      error_unexpected_identifier(state, transition_node_node);
      err = 2;
      goto end;
    }

    identifier = state_take_word(state);
    unsigned short key = keyword_get(identifier);
    switch(key) {
      case KW_GUARD: {
        token = consume_token(state);
        if(token != TOKEN_IDENTIFIER) {
          error_msg_with_code_block(state, NULL, "Expected a reference to an imported function after guard.");
          err = 2;
          goto end;
        }

        GuardExpression* guard_expression = node_create_guardexpression();
        guard_expression->ref = state_take_word(state);
        TransitionGuard* guard = node_transition_add_guard(transition_node, NULL);
        guard->expression = guard_expression;
        continue;
      }
    }

    if(state_has_guard(state, identifier)) {
      node_transition_add_guard(transition_node, identifier);
    } else if(state_has_action(state, identifier)) {
      node_transition_add_action(transition_node, identifier);
    } else {
      transition_node->dest = identifier;
    }
  }

  if(transition_node->dest == NULL) {
    transition_node->dest = state_node->name;
  }

  end: {
    state_node_up(state);
    return err;
  }
}

static int consume_invoke(State* state) {
  int err = 0;

  InvokeNode* invoke_node = node_create_invoke();
  Node* node = (Node*)invoke_node;
  state_node_start_pos(state, node, 6); // "invoke"

  Node* parent_node = state->node;
  if(parent_node->type != NODE_STATE_TYPE) {
    error_msg_with_code_block(state, node, "Unexpected parent for invoke.");
    return 2;
  }

  state_node_set(state, node);

  int token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_msg_with_code_block(state, node, "Expected a function to call with invoke.");
    return 2;
  }

  invoke_node->call = state_take_word(state);

  token = consume_token(state);

  if(token != TOKEN_BEGIN_BLOCK) {
    error_unexpected_identifier(state, node);
    return 2;
  }

  while(true) {
    token = consume_token(state);

    switch(token) {
      case TOKEN_EOL: continue;
      case TOKEN_END_BLOCK: {
        goto end;
      }
      case TOKEN_IDENTIFIER: {
        _check(consume_transition(state));
        break;
      }
      default: {
        error_unexpected_identifier(state, node);
        err = 2;
        goto end;
      }
    }
  }

  end: {
    state_node_up(state);
    return err;
  }
}

static int consume_state(State* state) {
  int err = 0;

  StateNode* state_node = node_create_state();
  Node* state_node_node = (Node*)state_node;
  state_node_start_pos(state, state_node_node, 5);

  Node* parent_node = state->node;
  if(parent_node->type != NODE_MACHINE_TYPE) {
    error_msg_with_code_block(state, state_node_node, "Unexpected parent node for state.");
    return 2;
  }

  state_node_set(state, state_node_node);

  int token = consume_token(state);

  switch(token) {
    case TOKEN_IDENTIFIER: {
      // Set the name of the state
      state_node->name = state_take_word(state);

      switch(state->modifier) {
        case MODIFIER_TYPE_INITIAL: {
          state->modifier = MODIFIER_NONE;
          MachineNode* machine_node = (MachineNode*)parent_node;
          machine_node->initial = strdup(state_node->name);
          break;
        }
        case MODIFIER_TYPE_FINAL: {
          state->modifier = MODIFIER_NONE;
          state_node->final = true;
          break;
        }
      }

      token = consume_token(state);
      break;
    }
    default: {
      err = 1;
      error_msg_with_code_block(state, state_node_node, "States must be given a name.");
      break;
    }
  }

  if(token != TOKEN_BEGIN_BLOCK) {
    error_unexpected_identifier(state, state_node_node);
    return 2;
  }

  while(true) {
    token = consume_token(state);

    switch(token) {
      case TOKEN_EOL: continue;
      case TOKEN_END_BLOCK: {
        state_node_node->end = state->index;
        goto end;
      };
      case TOKEN_CALL: {
        state_set_word(state, NULL);
        _check(consume_transition(state));
        break;
      }
      case TOKEN_IDENTIFIER: {
        unsigned short key = keyword_get(state->word);

        switch(key) {
          case KW_INVOKE: {
            _check(consume_invoke(state));
            break;
          }
          case KW_MACHINE: {
            _check(consume_machine(state));
            break;
          }
          default: {
            _check(consume_transition(state));
            break;
          }
        }

        break;
      }
      default: {
        error_unexpected_identifier(state, state_node_node);
        goto end;
      }
    }
  }

  end: {
    state_node_up(state);
    return err;
  }
}

static int consume_import_specifiers(ImportNode* import_node, State* state) {
  int err = 0;
  int token;

  ImportSpecifier *specifier;
  while(true) {
    token = consume_token(state);
    
    switch(token) {
      case TOKEN_IDENTIFIER: {
        char* identifier = state_take_word(state);
        if(strcmp(identifier, "as") == 0) {

          error_msg_with_code_block(state, (Node*)import_node, "Import aliases are not currently supported.");
          return 2;
        }

        specifier = node_create_import_specifier(identifier);
        break;
      }
      case TOKEN_END_BLOCK: {
        node_append((Node*)import_node, (Node*)specifier);
        goto end;
      }
      case TOKEN_UNKNOWN: {
        char c = state_char(state);

        // Getting into another specifier, close out this one.
        if(c == ',') {
          node_append((Node*)import_node, (Node*)specifier);
        }

        break;
      }
      default: {
        error_unexpected_identifier(state, (Node*)import_node);
        goto end;
      }
    }
  }

  end: {
    return err;
  }
}

static int consume_import(State* state) {
  int err = 0;
  Node *current_node = state->node;

  if(current_node != NULL) {
    error_msg_with_code_block(state, current_node, "Import statement must be at the top of the file.");
    return 2;
  }

  ImportNode *import_node = node_create_import_statement();
  Node *node = (Node*)import_node;
  state_node_start_pos(state, node, 6);
  state_node_set(state, node);

  int token;
  bool consumed_loc = false;
  bool consumed_specifiers = false;
  bool consumed_from = false;
  loop: while(true) {
    token = consume_token(state);

    switch(token) {
      case TOKEN_EOL: {
        if(consumed_loc) {
          goto end;
        }
      };
      case TOKEN_BEGIN_BLOCK: {
        _check(consume_import_specifiers(import_node, state));
        consumed_specifiers = true;

        break;
      }
      case TOKEN_IDENTIFIER: {
        if(strcmp(state->word, "from") == 0) {
          consumed_from = true;
          break;
        }
        error_unexpected_identifier(state, node);
        err = 2;
        goto end;
      }
      case TOKEN_STRING: {
        import_node->from = state_take_word(state);
        consumed_loc = true;
        goto loop;
      }
      case TOKEN_UNKNOWN: {
        error_unexpected_identifier(state, node);
        err = 2;
        goto end;
      }
      default: {
        error_unexpected_identifier(state, node);
        err = 2;
        goto end;
      }
    }
  }

  end: {
    state_node_up(state);
    return err;
  }
}

static int consume_action(State* state) {
  Assignment* assignment = node_create_assignment(ASSIGNMENT_ACTION);
  Node *node = (Node*)assignment;
  state_node_set(state, node);

  int token;
  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_unexpected_identifier(state, node);
    return 2;
  }

  char* binding_name = state_take_word(state);
  assignment->binding_name = binding_name;

  token = consume_token(state);

  if(token != TOKEN_ASSIGNMENT) {
    error_msg_with_code_block(state, node, "Expected an assignment");
    return 2;
  }

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_msg_with_code_block(state, node, "Expected an identifier");
    return 2;
  }

  if(keyword_get(state->word) != KW_ASSIGN) {
    error_msg_with_code_block(state, node, "Only assign expressions are supported at this time");
    return 2;
  }

  AssignExpression *expression = node_create_assignexpression();

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_unexpected_identifier(state, node);
    return 2;
  }

  expression->key = state_take_word(state);

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_unexpected_identifier(state, node);
    return 2;
  }

  expression->identifier = state_take_word(state);
  assignment->value = (Expression*)expression;

  state_add_action(state, assignment->binding_name);
  state_node_up(state);
  return 0;
}

static int consume_guard(State* state) {
  Assignment* assignment = node_create_assignment(ASSIGNMENT_GUARD);
  Node* node = (Node*)assignment;
  state_node_set(state, node);

  int token;

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_unexpected_identifier(state, node);
    return 2;
  }

  assignment->binding_name = state_take_word(state);

  token = consume_token(state);

  if(token != TOKEN_ASSIGNMENT) {
    error_msg_with_code_block(state, node, "Expected an identifier");
    return 2;
  }

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_msg_with_code_block(state, node, "Expected an identifier");
    return 2;
  }

  IdentifierExpression *expression = node_create_identifierexpression();
  expression->name = state_take_word(state);

  state_add_guard(state, assignment->binding_name);

  assignment->value = (Expression*)expression;
  state_node_up(state);
  return 0;
}

static int consume_machine_inner(State* state, bool is_implicit, int initial_token) {
  int err = 0;
  int token = is_implicit ? initial_token : consume_token(state);

  while(true) {
    switch(token) {
      case TOKEN_EOL: goto next;
      case TOKEN_EOF: goto end;
      case TOKEN_END_BLOCK: {
        if(!is_implicit) {
          goto end;
        }
        error_unexpected_identifier(state, state->node);
        err = 1;
        break;
      }
      case TOKEN_IDENTIFIER: {
        char* identifier = state->word;

        if(!is_keyword(identifier)) {
          error_msg_with_code_block(state, state->node, "Unknown top-level identifier.");
          err = 2;
          goto end;
        }

        unsigned short key = keyword_get(identifier);
        switch(key) {
          case KW_INITIAL: {
            state->modifier = MODIFIER_TYPE_INITIAL;
            break;
          }
          case KW_FINAL: {
            state->modifier = MODIFIER_TYPE_FINAL;
            break;
          }
          case KW_STATE: {
            _check(consume_state(state));
            break;
          }
          case KW_IMPORT: {
            _check(consume_import(state));
            break;
          }
          case KW_ACTION: {
            _check(consume_action(state));
            break;
          }
          case KW_GUARD: {
            _check(consume_guard(state));
            break;
          }
        }

        break;
      }
      default: {
        error_unexpected_identifier(state, state->node);
        err = 2;
        goto end;
      }
    }

    next: {
      token = consume_token(state);
    }
  }

  end: {
    return err;
  }
}

static int consume_machine(State* state) {
  int err = 0;

  MachineNode* machine_node = node_create_machine();
  Node* node = (Node*)machine_node;
  state_node_set(state, node);

  int token = consume_token(state);
  if(token != TOKEN_IDENTIFIER) {
    error_msg_with_code_block(state, node, "Machine must have a name.");
    err = 1;
    goto end;
  }
  machine_node->name = state_take_word(state);

  token = consume_token(state);

  if(token != TOKEN_BEGIN_BLOCK) {
    error_unexpected_identifier(state, node);
    err = 1;
    goto end;
  }

  _check(consume_machine_inner(state, false, 0));

  end: {
    state_node_up(state);
    return err;
  }
}

static int consume_implicit_machine(State* state, int current_token) {
  int err = 0;

  MachineNode* machine_node = node_create_machine();
  Node* node = (Node*)machine_node;

  state_node_set(state, node);

  _check(consume_machine_inner(state, true, current_token));

  state_node_up(state);
  return err;
}

static int consume_program(State* state) {
  int err = 0;
  Program* program = state->program;

  int token;

  while(true) {
    token = consume_token(state);

    switch(token) {
      case TOKEN_EOL: continue;
      case TOKEN_EOF: goto end;
      case TOKEN_IDENTIFIER: {
        char* identifier = state->word;

        if(!is_keyword(identifier)) {
          error_msg_with_code_block(state, state->node, "Unknown top-level identifier.");
          return 2;
        }

        unsigned short key = keyword_get(identifier);
        switch(key) {
          case KW_IMPORT: {
            _check(consume_import(state));
            break;
          }
          case KW_MACHINE: {
            _check(consume_machine(state));
            break;
          }
          default: {
            // Top-level machine
            _check(consume_implicit_machine(state, token));
            break;
          }
        }

        break;
      }
    }
  }

  end: {
    return err;
  }
}

ParseResult* parse(char* source, char* filename) {
  int err = 0;
  Program* program = new_program();
  State* state = state_new_state(source, filename);
  state->program = program;

  /*MachineNode* machine_node = node_create_machine();
  program->body = (Node*)machine_node;
  state->node = (Node*)machine_node;*/

  err = consume_program(state);

  ParseResult *result = malloc(sizeof(*result));
  result->success = err == 0;
  result->program = program;

  return result;
}

void parser_init() {
  keyword_init();
}