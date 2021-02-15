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

#define TOKEN_EOF 0
#define TOKEN_EOL 1
#define TOKEN_IDENTIFIER 2
#define TOKEN_ASSIGNMENT 3
#define TOKEN_CALL 4
#define TOKEN_BEGIN_BLOCK 5
#define TOKEN_END_BLOCK 6
#define TOKEN_STRING 7
#define TOKEN_UNKNOWN 8

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

static void consume_transition(State* state) {
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
      printf("A transition sibiling to another, hasn't happened before [%s]\n", event);
      node_after(current_node, transition_node_node);
      break;
    }
    default: {
      printf("Unexpected parent node to a transition\n");
      return;
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
      printf("Expected a call but got %i\n", token);
      return;
    }

    token = consume_token(state);

    if(token != TOKEN_IDENTIFIER) {
      printf("Expected an identifier\n");
      return;
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
}

static void consume_state(State* state) {
  StateNode* state_node = node_create_state();
  Node* state_node_node = (Node*)state_node;

  Node* parent_node = state->node;
  if(parent_node->type != NODE_MACHINE_TYPE) {
    printf("Unexpected parent node for state - %hu\n", parent_node->type);
    return;
  }

  switch(state->modifier) {
    case MODIFIER_TYPE_INITIAL: {
      state->modifier = MODIFIER_NONE;
      MachineNode* machine_node = (MachineNode*)parent_node;
      machine_node->initial = state_node->name;
      break;
    }
    case MODIFIER_TYPE_FINAL: {
      state_node->final = true;
      break;
    }
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

  if(token != TOKEN_IDENTIFIER) {
    printf("TODO error (consume_state).\n");
    return;
  }

  // Set the name of the state
  state_node->name = state->word;

  token = consume_token(state);

  if(token != TOKEN_BEGIN_BLOCK) {
    printf("TODO error (consume_state)\n");
    return;
  }

  while(1) {
    token = consume_token(state);

    switch(token) {
      case TOKEN_EOL: continue;
      case TOKEN_END_BLOCK: goto end;
      case TOKEN_IDENTIFIER: {
        consume_transition(state);
        break;
      }
      default: {
        printf("Unexpected token type (consume_state) - %i\n", token);
        goto end;
      }
    }
  }

  end: {
    state_node_up(state);
    return;
  }
}

static void consume_import_specifiers(ImportNode* import_node, State* state) {
  int token;
  ImportSpecifier *specifier;
  while(true) {
    token = consume_token(state);
    
    switch(token) {
      case TOKEN_IDENTIFIER: {
        char* identifier = state->word;
        if(strcmp(identifier, "as") == 0) {
          printf("This is an import as\n");
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
        printf("Unexpected token %i (consume_import_specifiers)\n", token);
        goto end;
      }
    }
  }

  end: {

  }
}

static void consume_import(State* state) {
  Node *current_node = state->node;

  if(current_node->type != NODE_MACHINE_TYPE) {
    // TODO other checks for this.
    printf("Import statement must be at the top of the file.\n");
  }

  ImportNode *node = node_create_import_statement();
  node_append(current_node, (Node*)node);

  int token;
  bool consumed_loc = false;
  bool consumed_specifiers = false;
  bool consumed_from = false;
  loop: while(1) {
    token = consume_token(state);

    switch(token) {
      case TOKEN_EOL: {
        if(consumed_loc) {
          goto end;
        }
      };
      case TOKEN_BEGIN_BLOCK: {
        consume_import_specifiers(node, state);
        consumed_specifiers = true;

        break;
      }
      case TOKEN_IDENTIFIER: {
        if(strcmp(state->word, "from") == 0) {
          consumed_from = true;
          break;
        }
        printf("Unexpected identifier [%s]\n", state->word);
        break;
      }
      case TOKEN_STRING: {
        node->from = state->word;
        consumed_loc = true;
        goto loop;
      }
      case TOKEN_UNKNOWN: {
        printf("Unexpected token %i (consume_import)\n", token);
        printf("Char is %c\n", state_char(state));
        goto end;
      }
      default: {
        printf("Unexexpected token %i (consume_import)\n", token);
        goto end;
      }
    }
  }

  end: {

  }
}

static void consume_action(State* state) {
  Assignment* assignment = node_create_assignment(ASSIGNMENT_ACTION);

  int token;

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    printf("This should be an identifier\n");
    return;
  }

  char* binding_name = state->word;
  assignment->binding_name = binding_name;

  token = consume_token(state);

  if(token != TOKEN_ASSIGNMENT) {
    printf("Expected an assignment\n");
    return;
  }

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    printf("Expected an identifier\n");
    return;
  }

  if(keyword_get(state->word) != KW_ASSIGN) {
    printf("only assign expressions are supported at this time\n");
    return;
  }

  AssignExpression *expression = node_create_assignexpression();

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    printf("Expected an identifier\n");
    return;
  }

  expression->key = state->word;

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    printf("Expected an identifier\n");
    return;
  }

  expression->identifier = state->word;
  assignment->value = (Expression*)expression;

  state_add_action(state, assignment->binding_name);
  node_append(state->node, (Node*)assignment);
}

static void consume_guard(State* state) {
  Assignment* assignment = node_create_assignment(ASSIGNMENT_GUARD);

  int token;

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    printf("Expected an identifier in a guard but got %i \n", token);
  }

  assignment->binding_name = state->word;

  token = consume_token(state);

  if(token != TOKEN_ASSIGNMENT) {
    printf("Expected an assignment\n");
    return;
  }

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    printf("Expected an identifier\n");
    return;
  }

  IdentifierExpression *expression = node_create_identifierexpression();
  expression->name = state->word;

  state_add_guard(state, assignment->binding_name);

  assignment->value = (Expression*)expression;
  node_append(state->node, (Node*)assignment);
}

static void consume_machine_inner(State* state) {
  int token;

  while(true) {
    token = consume_token(state);

    switch(token) {
      case TOKEN_EOL: continue;
      case TOKEN_EOF: goto end;
      case TOKEN_IDENTIFIER: {
        char* identifier = state->word;

        if(!is_keyword(identifier)) {
          printf("Unknown top-level identifier [%s] (consume_machine)\n", identifier);
          return;
        }

        unsigned short key = keyword_get(identifier);
        switch(key) {
          case KW_INITIAL: {
            state->modifier = MODIFIER_TYPE_INITIAL;
            break;
          }
          case KW_STATE: {
            consume_state(state);
            break;
          }
          case KW_IMPORT: {
            consume_import(state);
            break;
          }
          case KW_ACTION: {
            consume_action(state);
            break;
          }
          case KW_GUARD: {
            consume_guard(state);
            break;
          }
        }

        break;
      }
      default: {
        printf("Unexpected token type (consume_machine) - %i - %c\n", token, state_char(state));
        return;
      }
    }
  }

  end: {
    return;
  }
}

Program * parse(char * source) {
  State* state = state_new_state(source);
  Program* program = new_program();

  MachineNode* machine_node = node_create_machine();
  program->body = (Node*)machine_node;
  state->node = (Node*)machine_node;

  consume_machine_inner(state);

  return program;
}

void parser_init() {
  keyword_init();
}