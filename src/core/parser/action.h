#pragma once

#include "../node.h"
#include "../state.h"

int parser_consume_inline_action(State*, Node*);
int parser_consume_action(State*);