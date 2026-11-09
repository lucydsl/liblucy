#pragma once

#include "lexer.h"

#define _check(f) { int _fa = f; if(_fa == 2)  { return 2; } else if(_fa > err) { err = _fa; } }

typedef int (*consume_call_expr_args)(State*, void*, int, char*, int);

int consume_token(State*);
int consume_call_expression(State*, const char*, void*, consume_call_expr_args on_args);

/*
 * Check that a call such as guard(x) got the argument it needs, arg being
 * what the argument callback stored (NULL if nothing). err is the result
 * of consume_call_expression. With no argument this reports an error,
 * unless the call already reported one, and returns non-zero.
 */
int expect_call_argument(State*, const char* fn_name, const void* arg, int err);