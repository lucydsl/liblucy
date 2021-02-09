#include <stdlib.h>
#include <string.h>
#include "node.h"
#include "scope.h"
#include "state.h"

State* state_new_state(char* source)
{
  State *state = malloc(sizeof *state);
  state->source = source;
  state->source_len = strlen(source);
  state->index = 0;

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
  state->index++;
  return state_char(state);
}

char state_prev(State* state) {
  state->index--;
  return state_char(state);
}

void state_node_up(State* state) {
  Node* current = state->parent_node;
  Node* parent = current->parent;

  state->node = current;
  state->parent_node = parent;
}