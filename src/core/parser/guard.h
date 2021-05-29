#pragma once

#include "../node.h"
#include "../state.h"

int parser_consume_inline_guard(State*, TransitionNode*);
int parser_consume_guard(State*);