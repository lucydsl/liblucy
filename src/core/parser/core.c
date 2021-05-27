#include <stdio.h>
#include "../error.h"
#include "../identifier.h"
#include "../node.h"
#include "../state.h"
#include "../str_builder.h"
#include "../timeframe.h"
#include "core.h"
#include "token.h"

static inline int is_newline(char c) {
  return c == '\n';
}

static inline int is_whitespace(char c) {
  return c == ' ';
}

typedef bool (*consume_condition)(State*, char);

static void consume_while(State* state, consume_condition cond) {
  char c = state_char(state);

  str_builder_t *sb;
  sb = str_builder_create();
  size_t len = 0;

  do {
    str_builder_add_char(sb, c);
    len++;

    state_advance_column(state);
    state_next(state);

    c = state_char(state);
  } while(state_inbounds(state) && cond(state, c));
  state_rewind(state, 1);

  if(is_newline(c)) {
    state_advance_line(state);
  }

  char* str = str_builder_dump(sb, NULL);
  state_set_word(state, str);
  state->word_len = len;
  state->token_len = len;
  str_builder_destroy(sb);
}

static char* consume_string(State* state) {
  char c = state_char(state);
  char quote = c;
  size_t len = 0;

  str_builder_t *sb;
  sb = str_builder_create();

  do {
    str_builder_add_char(sb, c);
    len++;

    state_advance_column(state);
    state_next(state);


    c = state_char(state);
  } while(c != quote && c != '\n');

  str_builder_add_char(sb, quote);

  char* str = str_builder_dump(sb, &len);
  state_set_word(state, str);
  str_builder_destroy(sb);
  
  return str;
}

static bool identifier_consume_condition(State* state, char c) {
  return is_valid_identifier_char(c);
}

static void consume_identifier(State* state) {
  consume_while(state, &identifier_consume_condition);
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

int consume_call_expression(State* state, const char* fn_name, void* expr, consume_call_expr_args on_args) {
  int err = 0;
  int token = consume_token(state);

  bool in_call = false;
  if(token != TOKEN_BEGIN_CALL) {
    if(token == TOKEN_IDENTIFIER) {
      char buffer[100];
      sprintf(buffer, "%s must be called like a function. Expected %s(%s)", fn_name, fn_name, state->word);
      error_msg_with_code_block_dec(state, state->word_len, buffer);
    } else if(token == TOKEN_CALL) {
      char buffer[100];
      sprintf(buffer, "%s must be called like a function.", fn_name);
      error_msg_with_code_block_dec(state, state->token_len, buffer);
    } else {
      // What should this be?
      error_msg_with_code_block(state, NULL, "Expected a (");
    }
    err = 1;
  } else {
    in_call = true;
    token = consume_token(state);
  }

  char* identifier = NULL;
  int argi = 0;
  while(token != TOKEN_END_CALL) {
    switch(token) {
      case TOKEN_COMMA: break;
      case TOKEN_IDENTIFIER:
      case TOKEN_INTEGER:
      case TOKEN_SYMBOL:
      case TOKEN_TIMEFRAME: {
        identifier = state_take_word(state);
        _check(on_args(state, expr, token, identifier, argi));
        break;
      }
      default: {
        if(in_call) {
          error_msg_with_code_block_dec(state, state->token_len, "Unknown function argument.");
          err = 2;
          goto end;
        }
        goto end_args;
      }
    }
    token = consume_token(state);
    argi++;
  }

  end_args: {
    if(token != TOKEN_END_CALL) {
      // Only show this error message if we are currently within a call.
      if(in_call) {
        char buffer[100];
        char* key = identifier == NULL ? "" : identifier;
        sprintf(buffer, "Inline assigns must be called like a function. Expected assign(%s)", key);
        error_msg_with_code_block_dec(state, state->token_len, buffer);
      }

      // Rewind back out, so the next thing consumes this token.
      state_rewind(state, state->token_len);

      err = 1;
    }
  }

  end: {
    return err;
  }
}
