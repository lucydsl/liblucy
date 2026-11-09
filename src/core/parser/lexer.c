#include "internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Whether c can appear in an identifier: ASCII letters and underscore. */
static int is_valid_identifier_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static inline int is_newline(char c) {
  return c == '\n';
}

static inline int is_whitespace(char c) {
  return c == ' ';
}

typedef bool (*consume_condition)(State*, char);

static void consume_while(State* state, consume_condition cond) {
  char c = state_char(state);
  size_t start = state->index;
  size_t len = 0;

  // Only a ':' at the very end of the source gets here with nothing left
  // to read. Give it an empty word rather than stepping past the end.
  if(!state_inbounds(state)) {
    state_set_word(state, arena_strdup(state->arena, ""), start, start);
    state->word_len = 0;
    state->token_len = 0;
    return;
  }

  do {
    len++;

    state_advance_column(state);
    state_next(state);

    c = state_char(state);
  } while(state_inbounds(state) && cond(state, c));
  state_rewind(state, 1);

  if(is_newline(c)) {
    state_advance_line(state);
  }

  size_t end = state->index + 1;
  char* str = arena_strndup(state->arena, state->source + start, len);
  state_set_word(state, str, start, end);
  state->word_len = len;
  state->token_len = len;
}

static char* consume_string(State* state) {
  char c = state_char(state);
  char quote = c;
  size_t start = state->index;
  size_t len = 0;

  do {
    len++;

    state_advance_column(state);
    state_next(state);


    c = state_char(state);
  } while(c != quote && c != '\n' && state_inbounds(state));

  // The word is the string with its quotes, closing quote included even if
  // the string ran into the end of the line instead.
  char* str = arena_alloc(state->arena, len + 2);
  memcpy(str, state->source + start, len);
  str[len] = quote;

  size_t end = state->index;
  state_set_word(state, str, start, end);

  return str;
}

static bool identifier_consume_condition(State* state, char c) {
  return is_valid_identifier_char(c);
}

static void consume_identifier(State* state) {
  consume_while(state, &identifier_consume_condition);
}

static bool member_consume_condition(State* state, char c) {
  return is_valid_identifier_char(c) || c == '.';
}

static bool timeframe_consume_condition(State* state, char c) {
  return is_timeframe_char(c);
}

static void consume_timeframe(State* state) {
  consume_while(state, &timeframe_consume_condition);
}

/* Public API */
int consume_token(State* state) {
  int len = 0;
  while(state_inbounds(state)) {
    state_next(state);
    state_advance_column(state);
    char c = state_char(state);

    if(is_whitespace(c)) {
      continue;
    }

    if(is_newline(c)) {
      state_advance_line(state);
      state->token_len = 1;
      return TOKEN_EOL;
    }

    len++;

    if(c == '{') {
      state->token_len = 1;
      return TOKEN_BEGIN_BLOCK;
    }

    if(c == '}') {
      state->token_len = 1;
      return TOKEN_END_BLOCK;
    }

    if(c == '(') {
      state->token_len = 1;
      return TOKEN_BEGIN_CALL;
    }

    if(c == ')') {
      state->token_len = 1;
      return TOKEN_END_CALL;
    }

    if(c == '=') {
      if(state_peek(state) == '>') {
        state_next(state);
        state->token_len = 2;
        return TOKEN_CALL;
      }
      state->token_len = 1;
      return TOKEN_ASSIGNMENT;
    }

    if(is_valid_identifier_char(c)) {
      consume_identifier(state);
      if(state_peek(state) == '.') {
        return TOKEN_MEMBER_EXPRESSION;
      }
      return TOKEN_IDENTIFIER;
    }

    if(c == ':') {
      state_advance_column(state);
      state_next(state);
      consume_identifier(state);
      return TOKEN_SYMBOL;
    }

    if(c == '\'' || c == '"') {
      consume_string(state);
      return TOKEN_STRING;
    }

    if(c == ',') {
      state->token_len = 1;
      return TOKEN_COMMA;
    }

    if(c == '\0') {
      state->token_len = 1;
      return TOKEN_EOF;
    }

    if(c == '@') {
      consume_identifier(state);
      return TOKEN_LOCAL;
    }

    if(is_integer(c)) {
      consume_timeframe(state);
      char last = state->word[state->word_len - 1];
      if(is_integer(last)) {
        return TOKEN_INTEGER;
      } else {
        return TOKEN_TIMEFRAME;
      }
    }

    if(c == '/') {
      char nc = state_peek(state);
      if(nc == '/') {
        // single-line
        do {
          state_next(state);
          state_advance_column(state);
          nc = state_char(state);
        } while(state_inbounds(state) && !is_newline(nc));
        if(is_newline(state_char(state))) {
          state_advance_line(state);
        }
        continue;
      } else if(nc == '*') {
        // multi-line
        char lc = c;
        do {
          state_next(state);
          state_advance_column(state);
          nc = state_char(state);
          if(is_newline(nc)) {
            state_advance_line(state);
          }
        } while(state_inbounds(state) && (lc != '*' && nc != '/'));
        continue;
      }
    }

    state->token_len = len;
    return TOKEN_UNKNOWN;
  }

  state->token_len = len;
  return TOKEN_EOF;
}
