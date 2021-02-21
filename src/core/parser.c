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
  char* event = state->word;

  TransitionNode* transition_node = node_create_transition();
  transition_node->event = event;
  state_reset_word(state);

  // Parent should be a state node, should we check here? TODO
  Node* current_node = state->node;
  size_t current_node_type = current_node->type;

  Node* transition_node_node = (Node*)transition_node;

  switch(current_node_type) {
    case NODE_STATE_TYPE: {
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

  char* identifier = NULL;
  while(true) {
    int token = consume_token(state);

    if(token == TOKEN_EOL) {
      break;
    }

    if(token != TOKEN_CALL) {
      error_msg_with_code_block(state, transition_node_node, "Expected a call.");
      return 2;
    }

    token = consume_token(state);

    if(token != TOKEN_IDENTIFIER) {
      error_unexpected_identifier(state, transition_node_node);
      return 2;
    }

    // TODO if we already have a destination then someting went wrong, throw.

    identifier = state->word;
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

  state_node_up(state);
  return err;
}

int consume_state(State* state) {
  int err = 0;

  StateNode* state_node = node_create_state();
  Node* state_node_node = (Node*)state_node;
  state_node_start_pos(state, state_node_node);

  Node* parent_node = state->node;
  if(parent_node->type != NODE_MACHINE_TYPE) {
    error_msg_with_code_block(state, state_node_node, "Unexpected parent node for state.");
    return 2;
  }

  state->parent_node = parent_node;
  state->node = state_node_node;

  // TODO get rid of this..
  if(parent_node->type == NODE_STATE_TYPE) {
    node_after(parent_node, state_node_node);
  } else {
    node_append(parent_node, state_node_node);
  }

  int token = consume_token(state);

  switch(token) {
    case TOKEN_IDENTIFIER: {
      // Set the name of the state
      state_node->name = state->word;

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
      case TOKEN_IDENTIFIER: {
        _check(consume_transition(state));
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
        char* identifier = state->word;
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

  if(current_node->type != NODE_MACHINE_TYPE) {
    error_msg_with_code_block(state, current_node, "Import statement must be at the top of the file.");
    return 2;
  }

  ImportNode *node = node_create_import_statement();
  Node *import_node_node = (Node*)node;
  state_node_start_pos(state, import_node_node);

  node_append(current_node, import_node_node);

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
        _check(consume_import_specifiers(node, state));
        consumed_specifiers = true;

        break;
      }
      case TOKEN_IDENTIFIER: {
        if(strcmp(state->word, "from") == 0) {
          consumed_from = true;
          break;
        }
        error_unexpected_identifier(state, import_node_node);
        return 2;
      }
      case TOKEN_STRING: {
        node->from = state->word;
        consumed_loc = true;
        goto loop;
      }
      case TOKEN_UNKNOWN: {
        error_unexpected_identifier(state, import_node_node);
        goto end;
      }
      default: {
        error_unexpected_identifier(state, import_node_node);
        goto end;
      }
    }
  }

  end: {
    return err;
  }
}

static int consume_action(State* state) {
  Assignment* assignment = node_create_assignment(ASSIGNMENT_ACTION);
  Node *node = (Node*)assignment;

  int token;
  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_unexpected_identifier(state, node);
    return 2;
  }

  char* binding_name = state->word;
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

  expression->key = state->word;

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_unexpected_identifier(state, node);
    return 2;
  }

  expression->identifier = state->word;
  assignment->value = (Expression*)expression;

  state_add_action(state, assignment->binding_name);
  node_append(state->node, (Node*)assignment);
  return 0;
}

static int consume_guard(State* state) {
  Assignment* assignment = node_create_assignment(ASSIGNMENT_GUARD);
  Node* node = (Node*)assignment;

  int token;

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_unexpected_identifier(state, node);
    return 2;
  }

  assignment->binding_name = state->word;

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
  expression->name = state->word;

  state_add_guard(state, assignment->binding_name);

  assignment->value = (Expression*)expression;
  node_append(state->node, (Node*)assignment);
  return 0;
}

static int consume_machine_inner(State* state) {
  int err = 0;
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
          case KW_INITIAL: {
            state->modifier = MODIFIER_TYPE_INITIAL;
            break;
          }
          case KW_STATE: {
            //consume_state(state);
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
        return 2;
      }
    }
  }

  end: {
    return err;
  }
}

ParseResult* parse(char* source, char* filename) {
  int err = 0;
  State* state = state_new_state(source, filename);
  Program* program = new_program();

  MachineNode* machine_node = node_create_machine();
  program->body = (Node*)machine_node;
  state->node = (Node*)machine_node;

  err = consume_machine_inner(state);

  ParseResult *result = malloc(sizeof(*result));
  result->success = err == 0;
  result->program = program;

  return result;
}

void parser_init() {
  keyword_init();
}