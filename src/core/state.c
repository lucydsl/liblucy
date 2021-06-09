#include <stdio.h> // TODO remove
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "node.h"
#include "state.h"

State* state_new_state(char* source, char* filename) {
  State *state = malloc(sizeof *state);
  state->source = source;
  state->filename = filename;
  state->source_len = strlen(source);
  state->index = 0;
  state->started = false;

  state->guards = malloc(sizeof(SimpleSet));
  set_init(state->guards);
  state->actions = malloc(sizeof(SimpleSet));
  set_init(state->actions);

  state->node = NULL;
  state->parent_node = NULL;
  state->current_machine_node = NULL;

  state->word = NULL;
  state->line = 0;
  state->column = 0;
  return state;
}

void state_set_word(State* state, char* word) {
  state_reset_word(state);
  state->word = word;
}

char* state_take_word(State* state) {
  char* word = state->word;
  state->word = NULL;
  return word;
}

void state_reset_word(State* state) {
  if(state->word != NULL) {
    free(state->word);
    state->word = NULL;
  }
}

void state_advance_column(State* state) {
  state->column++;
}

void state_advance_line(State* state) {
  state->line++;
  state->column = 0;
}

int state_inbounds(State* state) {
  return state->index < state->source_len;
}

static inline char state_char_at(State* state, unsigned short index) {
  return state->source[index];
}

char state_char(State* state) {
  return state->source[state->index];
}

char state_peek(State* state) {
  return state->source[state->index + 1];
}

char state_next(State* state) {
  if(!state->started) {
    state->started = true;
  } else {
    state->index++;
  }

  return state_char(state);
}

void state_find_position(State* state, pos_t* pos, int inc) {
  size_t index = state->index;
  int col = 0;
  int line = state->line;
  int i = inc;
  char c;
  while(index > 0) {
    c = state_char_at(state, index);

    if(i <= 0) {
      col++;
    }

    if(c == '\n') {
      if(i <= 0) {
        // Reached the beginning of the line.
        break;
      } else {
        line--;
      }
    }

    i--;
    index--;
  }

  pos->line = line;
  pos->column = col;
}

void state_rewind(State* state, int inc) {
  pos_t pos;
  state_find_position(state, &pos, inc);
  state->index = state->index - inc;
  state->line = pos.line;
  state->column = pos.column;
}

void state_node_set(State* state, Node* node) {
  if(state->node != NULL) {
    Node* parent_node = state->node;
    state->parent_node = parent_node;

    node_append(parent_node, node);
  } else {
    Program* program = state->program;
    if(program->body == NULL) {
      program->body = node;
    } else {
      node_after_last(program->body, node);
    }
  }

  state->node = node;
}

void state_node_up(State* state) {
  Node* current_parent = state->parent_node;
  if(current_parent != NULL) {
    Node* new_parent = current_parent->parent;
    state->node = current_parent;
    state->parent_node = new_parent;
  } else {
    state->node = NULL;
  }
}

void state_node_start_pos(State* state, Node* node, unsigned short rewind_amount) {
  size_t start = state->index - rewind_amount;
  node->start = start;
  node->line = state->line;
}

void state_add_guard(State* state, char* name) {
  set_add(state->guards, name);
}

bool state_has_guard(State* state, char* name) {
  return set_contains(state->guards, name) == SET_TRUE;
}

void state_add_action(State* state, char* name) {
  set_add(state->actions, name);
}

bool state_has_action(State* state, char* name) {
  return set_contains(state->actions, name) == SET_TRUE;
}