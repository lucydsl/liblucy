#ifndef LUCY_STATE_H_
#define LUCY_STATE_H_

#include "scope.h"
#include "node.h"

#define MODIFIER_NONE 0
#define MODIFIER_TYPE_INITIAL 1
#define MODIFIER_TYPE_FINAL 2

typedef struct State {
  char* source;
  size_t source_len;
  size_t index;
  size_t line;
  size_t column;

  char* word;
  size_t in_word;
  size_t modifier;


  Node* node;
  Node* parent_node;

  Scope* scope;
} State;

int state_inbounds(State*);
char state_char(State*);
char state_peek(State*);
char state_next(State*);
char state_prev(State*);

State* state_new_state();
void state_advance_line(State*);
void state_advance_column(State*);
void state_set_word(State*, char*);
void state_reset_word(State*);
void state_node_up(State*);

#endif