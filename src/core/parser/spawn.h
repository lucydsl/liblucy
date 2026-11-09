#pragma once

#include "../ast/node.h"
#include "lexer.h"

int parser_consume_inline_spawn(State*, AssignExpression*);