#pragma once

/*
 * The parser's internals, shared by the files in parser/. The rest of the
 * compiler only needs parser.h.
 */

#include <stdbool.h>
#include <stddef.h>

#include "../ast/node.h"
#include "../ast/program.h"
#include "../support/arena.h"
#include "../support/str_set.h"

// The keyword before a state: `initial state a` or `final state b`.
typedef enum Modifier {
  MODIFIER_NONE,
  MODIFIER_INITIAL,
  MODIFIER_FINAL,
} Modifier;

/*
 * The parser's state for one parse. It lives on parse()'s stack and is set
 * up with parser_init. Words, nodes and everything else it creates come
 * from arena, which belongs to the caller and outlives the parse, so there
 * is nothing to tear down.
 */
typedef struct Parser {
  Arena* arena;
  char* source;
  char* filename;
  size_t source_len;
  size_t index;
  size_t line;
  size_t column;
  bool started;

  Modifier modifier;
  size_t word_start;
  size_t word_end;
  // The current word, an arena string. parser_take_word hands it to a node.
  char* word;
  size_t word_len;
  size_t token_len;

  // The names of the guards and actions declared so far.
  StrSet guard_names;
  StrSet action_names;

  Program* program;
  Node* node;
  Node* parent_node;
  // How many machines enclose the parser now. See MAX_MACHINE_DEPTH.
  size_t machine_depth;
} Parser;

// A position for an error message. Signed, because the lexer's line count
// can briefly go below zero while it rewinds.
typedef struct Position {
  long line;
  long column;
} Position;

/* lexer.c: the cursor, words and tokens. */

typedef enum TokenType {
  TOKEN_EOF,
  TOKEN_EOL,               // a newline
  TOKEN_IDENTIFIER,        // a word, keywords included
  TOKEN_ASSIGNMENT,        // =
  TOKEN_CALL,              // =>
  TOKEN_BEGIN_BLOCK,       // {
  TOKEN_END_BLOCK,         // }
  TOKEN_STRING,            // 'text' or "text"
  TOKEN_INTEGER,           // 200
  TOKEN_TIMEFRAME,         // 2s
  TOKEN_UNKNOWN,
  TOKEN_BEGIN_CALL,        // (
  TOKEN_END_CALL,          // )
  TOKEN_COMMA,             // ,
  TOKEN_LOCAL,             // @entry
  TOKEN_SYMBOL,            // :name
  TOKEN_MEMBER_EXPRESSION, // the first word of a.b
} TokenType;

int parser_in_bounds(Parser*);
char parser_char(Parser*);
char parser_peek(Parser*);
char parser_next(Parser*);
void parser_rewind(Parser*, size_t);
void parser_find_position(Parser*, Position*, size_t);

void parser_init(Parser*, Arena*, Program*, char* source, char* filename);
void parser_advance_line(Parser*);
void parser_advance_column(Parser*);
void parser_set_word(Parser*, char*, size_t, size_t);
char* parser_take_word(Parser*);
void parser_reset_word(Parser*);
void parser_enter_node(Parser*, Node*);
void parser_leave_node(Parser*);
void parser_node_start_pos(Parser*, Node*, size_t);

void parser_add_guard_name(Parser*, char*);
void parser_add_action_name(Parser*, char*);
bool parser_has_guard_name(Parser*, char*);
bool parser_has_action_name(Parser*, char*);

TokenType parser_next_token(Parser*);

/* call.c: calls such as guard(x). */

#define _check(f) { int _fa = f; if(_fa == 2)  { return 2; } else if(_fa > err) { err = _fa; } }

typedef int (*CallArgHandler)(Parser*, void*, TokenType, char*, int);

int parser_consume_call(Parser*, const char*, void*, CallArgHandler on_args);

/*
 * Check that a call such as guard(x) got the argument it needs, arg being
 * what the argument callback stored (NULL if nothing). err is the result
 * of parser_consume_call. With no argument this reports an error,
 * unless the call already reported one, and returns non-zero.
 */
int parser_expect_call_argument(Parser*, const char* fn_name, const void* arg, int err);

/* error.c: error messages pointing into the source. */

void error_message(const char*);
void parser_error(Parser*, Node*, const char*);
void parser_error_at(Parser*, Position*, const char*);
void parser_error_back(Parser*, size_t, const char*);
void parser_error_unexpected_identifier(Parser*, Node*);

/* keyword.c: keywords and @entry/@exit. */

typedef enum Keyword {
  KEYWORD_NONE,  // not a keyword
  KEYWORD_USE,
  KEYWORD_STATE,
  KEYWORD_INITIAL,
  KEYWORD_FINAL,
  KEYWORD_ACTION,
  KEYWORD_GUARD,
  KEYWORD_ASSIGN,
  KEYWORD_INVOKE,
  KEYWORD_MACHINE,
  KEYWORD_DELAY,
  KEYWORD_ON,
  KEYWORD_SPAWN,
  KEYWORD_SEND,
} Keyword;

/* The keyword a word is, or KEYWORD_NONE (also for NULL). */
Keyword keyword_lookup(const char*);

/* LOCAL_ENTRY or LOCAL_EXIT for "@entry" or "@exit", otherwise LOCAL_NONE. */
LocalKind local_lookup(const char*);
const char* local_name(LocalKind);

/* timeframe.c: delays such as 200 or 2s. */

typedef struct Timeframe {
  bool timeframe_is_digit;
  int time;
  char* error;
} Timeframe;

bool timeframe_is_digit(char);
bool timeframe_is_char(char);
bool timeframe_is_suffix(const char*);
Timeframe timeframe_parse(char*, size_t);

/* The parsers for each construct, one file each. */

int parser_consume_machine(Parser*);
int parser_consume_implicit_machine(Parser*, TokenType);
int parser_consume_state(Parser*);
int parser_consume_use(Parser*);
int parser_consume_local(Parser*, LocalKind);
int parser_consume_transition(Parser*);
int parser_consume_invoke(Parser*);
int parser_consume_action(Parser*);
int parser_consume_inline_action(Parser*, Node*);
int parser_consume_guard(Parser*);
int parser_consume_inline_guard(Parser*, TransitionNode*);
int parser_consume_inline_assign_args(Parser*, void*, TokenType, char*, int);
int parser_consume_inline_assign(Parser*, Node*);
int parser_consume_inline_send_args(Parser*, void*, TokenType, char*, int);
int parser_consume_inline_send(Parser*, Node*);
int parser_consume_inline_spawn(Parser*, AssignExpression*);
int parser_consume_inline_delay(Parser*, TransitionNode*, StateNode*);
