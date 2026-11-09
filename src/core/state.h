#pragma once

#include "arena.h"
#include "node.h"
#include "str_set.h"
#include "program.h"

#define MODIFIER_NONE 0
#define MODIFIER_TYPE_INITIAL 1
#define MODIFIER_TYPE_FINAL 2

/*
 * The parser's state for one parse. It lives on parse()'s stack and is set
 * up with state_init. Words, nodes and everything else it creates come
 * from arena, which belongs to the caller and outlives the parse, so there
 * is nothing to tear down.
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

  str_set_t guards;
  str_set_t actions;

  Program* program;
  Node* node;
  Node* parent_node;
  // How many machines enclose the parser now. See MAX_MACHINE_DEPTH.
  size_t machine_depth;
} State;

// A position for an error message. Signed, because the lexer's line count
// can briefly go below zero while it rewinds.
typedef struct pos_t {
  long line;
  long column;
} pos_t;

int state_inbounds(State*);
char state_char(State*);
char state_peek(State*);
char state_next(State*);
void state_rewind(State*, size_t);
void state_find_position(State*, pos_t*, size_t);

void state_init(State*, Arena*, Program*, char* source, char* filename);
void state_advance_line(State*);
void state_advance_column(State*);
void state_set_word(State*, char*, size_t, size_t);
char* state_take_word(State*);
void state_reset_word(State*);
void state_node_set(State*, Node*);
void state_node_up(State*);
void state_node_start_pos(State*, Node*, size_t);

void state_add_guard(State*, char*);
void state_add_action(State*, char*);
bool state_has_guard(State*, char*);
bool state_has_action(State*, char*);