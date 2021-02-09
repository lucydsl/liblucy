#include <stdlib.h>
#include "node.h"
#include "scope.h"

typedef struct State
{
  char* word;
  int in_word;
  int line;
  int column;

  Node* node;
  Node* parent_node;

  Scope* scope;
} State;

State* state_new_state()
{
  State *state = malloc(sizeof *state);
  state->word = "";
  state->in_word = 0;
  state->line = 0;
  state->column = 0;
  return state;
}

int state_in_word(State* state)
{
  //return strcmp(state->word, "") != 0;
  return state->in_word;
}

void state_set_word(State* state, char* word)
{
  strcpy(state->word, word);
  state->in_word = 1;
}

void state_reset_word(State* state) 
{
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