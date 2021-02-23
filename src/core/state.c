#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "node.h"
#include "scope.h"
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

  state->word = "";
  state->in_word = 0;
  state->line = 0;
  state->column = 0;
  return state;
}

void state_set_word(State* state, char* word) {
  state->word = word;
  state->in_word = 1;
}

void state_reset_word(State* state) {
  state->word = "";
  state->in_word = 0;
}

void state_advance_column(State* state) {
  state->column++;
}

void state_advance_line(State* state) {
  state->line++;
  state->column++;
}

int state_inbounds(State* state) {
  return state->index < state->source_len;
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

char state_prev(State* state) {
  state->index--;
  return state_char(state);
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

void state_node_start_pos(State* state, Node* node) {
  size_t start = state->index - strlen(state->word);
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