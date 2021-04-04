#pragma once

#include "node.h"
#include "state.h"

void error_message(char*);
void error_msg_with_code_block(State*, Node*, const char*);
void error_msg_with_code_block_pos(State*, pos_t*, const char*);
void error_msg_with_code_block_dec(State*, int, const char*);
void error_unexpected_identifier(State*, Node*);