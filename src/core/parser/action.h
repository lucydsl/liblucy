#pragma once

#include "../ast/node.h"
#include "lexer.h"

int parser_consume_inline_action(State*, Node*);
int parser_consume_action(State*);