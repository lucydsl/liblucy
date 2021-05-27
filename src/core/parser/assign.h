#pragma once

#include "../node.h"
#include "../state.h"

int parser_consume_inline_assign_args(State*, void*, int, char*, int);
int parser_consume_inline_assign(State*, Node*);