#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../ast/node.h"
#include "lexer.h"

void state_init(State* state, Arena* arena, Program* program, char* source, char* filename) {
  // Zero everything first: the parser reads modifier and the word
  // fields before it ever sets them.
  memset(state, 0, sizeof(*state));
  state->arena = arena;
  state->program = program;
  state->source = source;
  state->filename = filename;
  state->source_len = strlen(source);

  str_set_init(&state->guards, arena);
  str_set_init(&state->actions, arena);
}

void state_set_word(State* state, char* word, size_t start, size_t end) {
  state->word = word;
  state->word_start = start;
  state->word_end = end;
}

char* state_take_word(State* state) {
  char* word = state->word;
  state->word = NULL;
  return word;
}

void state_reset_word(State* state) {
  state->word = NULL;
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

// Every read of the source goes through here. The lexer steps past the
// end in a few places (a ':' or an unterminated string at the end of the
// file), so anything past the end reads as the terminating NUL.
static inline char state_char_at(State* state, size_t index) {
  return index < state->source_len ? state->source[index] : '\0';
}

char state_char(State* state) {
  return state_char_at(state, state->index);
}

char state_peek(State* state) {
  return state_char_at(state, state->index + 1);
}

char state_next(State* state) {
  if(!state->started) {
    state->started = true;
  } else {
    state->index++;
  }

  return state_char(state);
}

// The line and column of the point inc characters back from the current
// index. inc is a token length, so it is a size_t: as an int, as it was, a
// token over INT_MAX characters went negative and state_rewind went back to
// the start of the source.
void state_find_position(State* state, pos_t* pos, size_t inc) {
  size_t index = state->index;
  long col = 0;
  long line = (long)state->line;
  // How far back from the current index this has looked.
  size_t back = 0;
  char c;
  while(index > 0) {
    c = state_char_at(state, index);

    if(back >= inc) {
      col++;
    }

    if(c == '\n') {
      if(back >= inc) {
        // Reached the beginning of the line.
        break;
      } else {
        line--;
      }
    }

    back++;
    index--;
  }

  pos->line = line;
  pos->column = col;
}

void state_rewind(State* state, size_t inc) {
  pos_t pos;
  state_find_position(state, &pos, inc);
  state->index = inc < state->index ? state->index - inc : 0;
  state->line = (size_t)pos.line;
  state->column = (size_t)pos.column;
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

void state_node_start_pos(State* state, Node* node, size_t rewind_amount) {
  // A keyword at the very start of the source is shorter than the amount
  // some callers rewind by, which wrapped start around to SIZE_MAX.
  node->start = rewind_amount < state->index ? state->index - rewind_amount : 0;
  node->line = state->line;
}

void state_add_guard(State* state, char* name) {
  str_set_add(&state->guards, name);
}

bool state_has_guard(State* state, char* name) {
  return str_set_contains(&state->guards, name);
}

void state_add_action(State* state, char* name) {
  str_set_add(&state->actions, name);
}

bool state_has_action(State* state, char* name) {
  return str_set_contains(&state->actions, name);
}