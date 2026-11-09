#pragma once

#include "../ast/node.h"
#include "lexer.h"

int parser_consume_inline_guard(State*, TransitionNode*);
int parser_consume_guard(State*);