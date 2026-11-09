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

  Modifier modifier;
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

TokenType consume_token(State*);

/* call.c: calls such as guard(x). */

#define _check(f) { int _fa = f; if(_fa == 2)  { return 2; } else if(_fa > err) { err = _fa; } }

typedef int (*consume_call_expr_args)(State*, void*, TokenType, char*, int);

int consume_call_expression(State*, const char*, void*, consume_call_expr_args on_args);

/*
 * Check that a call such as guard(x) got the argument it needs, arg being
 * what the argument callback stored (NULL if nothing). err is the result
 * of consume_call_expression. With no argument this reports an error,
 * unless the call already reported one, and returns non-zero.
 */
int expect_call_argument(State*, const char* fn_name, const void* arg, int err);

/* error.c: error messages pointing into the source. */

void error_message(const char*);
void error_msg_with_code_block(State*, Node*, const char*);
void error_msg_with_code_block_pos(State*, pos_t*, const char*);
void error_msg_with_code_block_dec(State*, size_t, const char*);
void error_unexpected_identifier(State*, Node*);

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

bool is_keyword(const char*);
/* The keyword a word is, or KEYWORD_NONE (also for NULL). */
Keyword keyword_get(const char*);

/* LOCAL_ENTRY or LOCAL_EXIT for "@entry" or "@exit", otherwise LOCAL_NONE. */
LocalKind local_get(const char*);
const char* local_get_name(LocalKind);

/* timeframe.c: delays such as 200 or 2s. */

typedef struct Timeframe {
  bool is_integer;
  int time;
  char* error;
} Timeframe;

bool is_integer(char);
bool is_timeframe_char(char);
bool timeframe_is_suffix(const char*);
Timeframe timeframe_parse(char*, size_t);

/* The parsers for each construct, one file each. */

int parser_consume_machine(State*);
int parser_consume_implicit_machine(State*, TokenType);
int parser_consume_state(State*);
int parser_consume_use(State*);
int parser_consume_local(State*, LocalKind);
int parser_consume_transition(State*);
int parser_consume_invoke(State*);
int parser_consume_action(State*);
int parser_consume_inline_action(State*, Node*);
int parser_consume_guard(State*);
int parser_consume_inline_guard(State*, TransitionNode*);
int parser_consume_inline_assign_args(State*, void*, TokenType, char*, int);
int parser_consume_inline_assign(State*, Node*);
int parser_consume_inline_send_args(State*, void*, TokenType, char*, int);
int parser_consume_inline_send(State*, Node*);
int parser_consume_inline_spawn(State*, AssignExpression*);
int parser_consume_inline_delay(State*, TransitionNode*, StateNode*);
