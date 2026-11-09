#include "internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void parser_init(Parser* parser, Arena* arena, Program* program, const char* source, const char* filename, DiagnosticList* diagnostics) {
  // Zero everything first: the parser reads modifier and the word
  // fields before it ever sets them.
  memset(parser, 0, sizeof(*parser));
  parser->arena = arena;
  parser->program = program;
  parser->source = source;
  parser->filename = filename;
  parser->diagnostics = diagnostics;
  parser->source_len = strlen(source);

  str_set_init(&parser->guard_names, arena);
  str_set_init(&parser->action_names, arena);
}

void parser_set_word(Parser* parser, const char* word, size_t start, size_t end) {
  parser->word = word;
  parser->word_start = start;
  parser->word_end = end;
}

const char* parser_take_word(Parser* parser) {
  const char* word = parser->word;
  parser->word = NULL;
  return word;
}

void parser_advance_column(Parser* parser) {
  parser->column++;
}

void parser_advance_line(Parser* parser) {
  parser->line++;
  parser->column = 0;
}

bool parser_in_bounds(Parser* parser) {
  return parser->index < parser->source_len;
}

// Every read of the source goes through here. The lexer steps past the
// end in a few places (a ':' or an unterminated string at the end of the
// file), so anything past the end reads as the terminating NUL.
static inline char char_at(Parser* parser, size_t index) {
  return index < parser->source_len ? parser->source[index] : '\0';
}

char parser_char(Parser* parser) {
  return char_at(parser, parser->index);
}

char parser_peek(Parser* parser) {
  return char_at(parser, parser->index + 1);
}

char parser_next(Parser* parser) {
  if(!parser->started) {
    parser->started = true;
  } else {
    parser->index++;
  }

  return parser_char(parser);
}

// The line and column of the point inc characters back from the current
// index. inc is a token length, so it is a size_t: as an int, as it was, a
// token over INT_MAX characters went negative and parser_rewind went back to
// the start of the source.
void parser_find_position(Parser* parser, Position* pos, size_t inc) {
  size_t index = parser->index;
  long col = 0;
  long line = (long)parser->line;
  // How far back from the current index this has looked.
  size_t back = 0;
  char c;
  while(index > 0) {
    c = char_at(parser, index);

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

void parser_rewind(Parser* parser, size_t inc) {
  Position pos;
  parser_find_position(parser, &pos, inc);
  parser->index = inc < parser->index ? parser->index - inc : 0;
  parser->line = (size_t)pos.line;
  parser->column = (size_t)pos.column;
}

void parser_enter_node(Parser* parser, Node* node) {
  if(parser->node != NULL) {
    Node* parent_node = parser->node;
    parser->parent_node = parent_node;

    node_append_child(parent_node, node);
  } else {
    Program* program = parser->program;
    if(program->body == NULL) {
      program->body = node;
    } else {
      node_append_sibling(program->body, node);
    }
  }

  parser->node = node;
}

void parser_leave_node(Parser* parser) {
  Node* current_parent = parser->parent_node;
  if(current_parent != NULL) {
    Node* new_parent = current_parent->parent;
    parser->node = current_parent;
    parser->parent_node = new_parent;
  } else {
    parser->node = NULL;
  }
}

void parser_add_guard_name(Parser* parser, const char* name) {
  str_set_add(&parser->guard_names, name);
}

bool parser_has_guard_name(Parser* parser, const char* name) {
  return str_set_contains(&parser->guard_names, name);
}

void parser_add_action_name(Parser* parser, const char* name) {
  str_set_add(&parser->action_names, name);
}

bool parser_has_action_name(Parser* parser, const char* name) {
  return str_set_contains(&parser->action_names, name);
}

/* Whether c can appear in an identifier: ASCII letters and underscore. */
static bool is_identifier_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static inline bool is_newline(char c) {
  return c == '\n';
}

static inline bool is_whitespace(char c) {
  return c == ' ';
}

typedef bool (*CharPredicate)(char);

static void consume_while(Parser* parser, CharPredicate cond) {
  char c = parser_char(parser);
  size_t start = parser->index;
  size_t len = 0;

  // Only a ':' at the very end of the source gets here with nothing left
  // to read. Give it an empty word rather than stepping past the end.
  if(!parser_in_bounds(parser)) {
    parser_set_word(parser, arena_strdup(parser->arena, ""), start, start);
    parser->word_len = 0;
    parser->token_len = 0;
    return;
  }

  do {
    len++;

    parser_advance_column(parser);
    parser_next(parser);

    c = parser_char(parser);
  } while(parser_in_bounds(parser) && cond(c));
  parser_rewind(parser, 1);

  if(is_newline(c)) {
    parser_advance_line(parser);
  }

  size_t end = parser->index + 1;
  char* str = arena_strndup(parser->arena, parser->source + start, len);
  parser_set_word(parser, str, start, end);
  parser->word_len = len;
  parser->token_len = len;
}

static char* consume_string(Parser* parser) {
  char c = parser_char(parser);
  char quote = c;
  size_t start = parser->index;
  size_t len = 0;

  do {
    len++;

    parser_advance_column(parser);
    parser_next(parser);


    c = parser_char(parser);
  } while(c != quote && c != '\n' && parser_in_bounds(parser));

  // The word is the string with its quotes, closing quote included even if
  // the string ran into the end of the line instead.
  char* str = arena_alloc(parser->arena, len + 2);
  memcpy(str, parser->source + start, len);
  str[len] = quote;

  size_t end = parser->index;
  parser_set_word(parser, str, start, end);

  return str;
}

static void consume_identifier(Parser* parser) {
  consume_while(parser, is_identifier_char);
}

static void consume_timeframe(Parser* parser) {
  consume_while(parser, timeframe_is_char);
}

/* Public API */
TokenType parser_next_token(Parser* parser) {
  size_t len = 0;
  while(parser_in_bounds(parser)) {
    parser_next(parser);
    parser_advance_column(parser);
    char c = parser_char(parser);

    if(is_whitespace(c)) {
      continue;
    }

    if(is_newline(c)) {
      parser_advance_line(parser);
      parser->token_len = 1;
      return TOKEN_EOL;
    }

    len++;

    if(c == '{') {
      parser->token_len = 1;
      return TOKEN_BEGIN_BLOCK;
    }

    if(c == '}') {
      parser->token_len = 1;
      return TOKEN_END_BLOCK;
    }

    if(c == '(') {
      parser->token_len = 1;
      return TOKEN_BEGIN_CALL;
    }

    if(c == ')') {
      parser->token_len = 1;
      return TOKEN_END_CALL;
    }

    if(c == '=') {
      if(parser_peek(parser) == '>') {
        parser_next(parser);
        parser->token_len = 2;
        return TOKEN_CALL;
      }
      parser->token_len = 1;
      return TOKEN_ASSIGNMENT;
    }

    if(is_identifier_char(c)) {
      consume_identifier(parser);
      if(parser_peek(parser) == '.') {
        return TOKEN_MEMBER_EXPRESSION;
      }
      return TOKEN_IDENTIFIER;
    }

    if(c == ':') {
      parser_advance_column(parser);
      parser_next(parser);
      consume_identifier(parser);
      return TOKEN_SYMBOL;
    }

    if(c == '\'' || c == '"') {
      consume_string(parser);
      return TOKEN_STRING;
    }

    if(c == ',') {
      parser->token_len = 1;
      return TOKEN_COMMA;
    }

    if(c == '\0') {
      parser->token_len = 1;
      return TOKEN_EOF;
    }

    if(c == '@') {
      consume_identifier(parser);
      return TOKEN_LOCAL;
    }

    if(timeframe_is_digit(c)) {
      consume_timeframe(parser);
      char last = parser->word[parser->word_len - 1];
      if(timeframe_is_digit(last)) {
        return TOKEN_INTEGER;
      } else {
        return TOKEN_TIMEFRAME;
      }
    }

    if(c == '/') {
      char nc = parser_peek(parser);
      if(nc == '/') {
        // single-line
        do {
          parser_next(parser);
          parser_advance_column(parser);
          nc = parser_char(parser);
        } while(parser_in_bounds(parser) && !is_newline(nc));
        if(is_newline(parser_char(parser))) {
          parser_advance_line(parser);
        }
        continue;
      } else if(nc == '*') {
        // multi-line
        // Known bug: lc is never updated, so the comment ends at its first
        // '/' rather than at "*/".
        char lc = c;
        do {
          parser_next(parser);
          parser_advance_column(parser);
          nc = parser_char(parser);
          if(is_newline(nc)) {
            parser_advance_line(parser);
          }
        } while(parser_in_bounds(parser) && (lc != '*' && nc != '/'));
        continue;
      }
    }

    parser->token_len = len;
    return TOKEN_UNKNOWN;
  }

  parser->token_len = len;
  return TOKEN_EOF;
}
