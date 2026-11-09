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
#include "../diagnostic.h"
#include "../support/str_builder.h"
#include "../support/str_set.h"

/*
 * What each parse function returns. By the time one returns PARSE_ERROR or
 * PARSE_FATAL it has reported the error.
 */
typedef enum ParseStatus {
  PARSE_OK,
  // There was an error, but the parser can carry on and look for more.
  PARSE_ERROR,
  // There was an error the parser can't carry on from: every caller
  // returns PARSE_FATAL at once, without tidying up.
  PARSE_FATAL,
} ParseStatus;

/*
 * Record result, what a nested parse returned, in *status, which keeps the
 * worst result so far, and return result. A caller stops at once when a
 * nested parse is fatal:
 *
 *   if(parse_merge(&status, parser_consume_state(parser)) == PARSE_FATAL) {
 *     return PARSE_FATAL;
 *   }
 */
static inline ParseStatus parse_merge(ParseStatus* status, ParseStatus result) {
  if(result > *status) {
    *status = result;
  }
  return result;
}

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

  // Where errors go. The compile's result owns it.
  DiagnosticList* diagnostics;
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

void parser_init(Parser*, Arena*, Program*, char* source, char* filename, DiagnosticList*);
void parser_advance_line(Parser*);
void parser_advance_column(Parser*);
void parser_set_word(Parser*, char*, size_t, size_t);
char* parser_take_word(Parser*);
void parser_enter_node(Parser*, Node*);
void parser_leave_node(Parser*);

void parser_add_guard_name(Parser*, char*);
void parser_add_action_name(Parser*, char*);
bool parser_has_guard_name(Parser*, char*);
bool parser_has_action_name(Parser*, char*);

TokenType parser_next_token(Parser*);

/* call.c: calls such as guard(x). */

/*
 * Called by parser_consume_call for each of a call's arguments: a word,
 * a number, a timeframe or a :symbol. expr is the expression the caller
 * passed, for the handler to fill in; index counts the arguments and
 * commas so far.
 */
typedef ParseStatus (*CallArgHandler)(Parser*, void* expr, TokenType, char* arg, size_t index);

/*
 * Parse a call's argument list, `(a, b)`, after its name (fn_name, for
 * error messages), passing each argument to on_args.
 */
ParseStatus parser_consume_call(Parser*, const char* fn_name, void* expr, CallArgHandler on_args);

/*
 * Check that a call such as guard(x) got the argument it needs, arg being
 * what the argument handler stored (NULL if nothing). status is what
 * parser_consume_call returned. With no argument this reports an error,
 * unless the call already reported one, and returns PARSE_ERROR.
 */
ParseStatus parser_expect_call_argument(Parser*, const char* fn_name, const void* arg, ParseStatus status);

/*
 * error.c: report an error in the source, adding it to the parse's
 * diagnostics. The parse functions then return PARSE_ERROR or PARSE_FATAL.
 */

/* At the current position. */
void parser_error(Parser*, const char* message);
/* At the position back characters before the current one. */
void parser_error_back(Parser*, size_t back, const char* message);
/* The same, with a message formatted as by printf and cut to 255 bytes. */
void parser_error_backf(Parser*, size_t back, const char* format, ...)
  STR_BUILDER_PRINTF(3, 4);
/* "Unexpected identifier", at the current position. */
void parser_error_unexpected_identifier(Parser*);

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
  // In milliseconds.
  int time;
  // Why the timeframe isn't valid, or NULL if it is.
  const char* error;
} Timeframe;

bool timeframe_is_digit(char);
/* Whether c can be part of a timeframe: a digit, or the s or m suffix. */
bool timeframe_is_char(char);
/* Parse the len characters of word as a timeframe. */
Timeframe timeframe_parse(const char* word, size_t len);

/* The parsers for each construct, one file each. */

ParseStatus parser_consume_machine(Parser*);
ParseStatus parser_consume_implicit_machine(Parser*, TokenType);
ParseStatus parser_consume_state(Parser*);
ParseStatus parser_consume_use(Parser*);
ParseStatus parser_consume_local(Parser*, LocalKind);
ParseStatus parser_consume_transition(Parser*);
ParseStatus parser_consume_invoke(Parser*);
ParseStatus parser_consume_action(Parser*);
ParseStatus parser_consume_inline_action(Parser*, Node*);
ParseStatus parser_consume_guard(Parser*);
ParseStatus parser_consume_inline_guard(Parser*, TransitionNode*);
ParseStatus parser_consume_inline_assign_args(Parser*, void*, TokenType, char*, size_t);
ParseStatus parser_consume_inline_assign(Parser*, Node*);
ParseStatus parser_consume_inline_send_args(Parser*, void*, TokenType, char*, size_t);
ParseStatus parser_consume_inline_send(Parser*, Node*);
ParseStatus parser_consume_inline_spawn(Parser*, AssignExpression*);
ParseStatus parser_consume_inline_delay(Parser*, TransitionNode*, StateNode*);
