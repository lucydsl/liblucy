#pragma once

#include "../state.h"

#define _check(f) { int _fa = f; if(_fa == 2)  { return 2; } else if(_fa > err) { err = _fa; } }

typedef int (*consume_call_expr_args)(State*, void*, int, char*, int);

int consume_token(State*);
int consume_call_expression(State*, const char*, void*, consume_call_expr_args on_args);