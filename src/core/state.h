#pragma once

#include "arena.h"
#include "node.h"
#include "set.h"
#include "program.h"

#define MODIFIER_NONE 0
#define MODIFIER_TYPE_INITIAL 1
#define MODIFIER_TYPE_FINAL 2

/*
 * The parser's state for one parse. It lives on parse()'s stack; set it
 * up with state_init and tear it down with state_destroy. Words, nodes and
 * everything else it creates come from arena, which belongs to the
 * caller and outlives the parse.
 */
typedef struct State {
  Arena* arena;
  char* source;
  char* filename;
  size_t source_len;
  size_t index;
  size_t line;
  size_t column;
  bool started;

  size_t modifier;
  size_t word_start;
  size_t word_end;
  // The current word, an arena string. state_take_word hands it to a node.
  char* word;
  size_t word_len;
  size_t token_len;

  SimpleSet guards;
  SimpleSet actions;

  Program* program;
  Node* node;
  Node* parent_node;
  MachineNode* current_machine_node;
} State;

typedef struct pos_t {
  int line;
  int column;
} pos_t;

int state_inbounds(State*);
char state_char(State*);
char state_peek(State*);
char state_next(State*);
void state_rewind(State*, int);
void state_find_position(State*, pos_t*, int);

void state_init(State*, Arena*, Program*, char* source, char* filename);
void state_destroy(State*);
void state_advance_line(State*);
void state_advance_column(State*);
void state_set_word(State*, char*, size_t, size_t);
char* state_take_word(State*);
void state_reset_word(State*);
void state_node_set(State*, Node*);
void state_node_up(State*);
void state_node_start_pos(State*, Node*, unsigned short);

void state_add_guard(State*, char*);
void state_add_action(State*, char*);
bool state_has_guard(State*, char*);
bool state_has_action(State*, char*);