#pragma once

#include "../ast/node.h"
#include "lexer.h"

void error_message(const char*);
void error_msg_with_code_block(State*, Node*, const char*);
void error_msg_with_code_block_pos(State*, pos_t*, const char*);
void error_msg_with_code_block_dec(State*, size_t, const char*);
void error_unexpected_identifier(State*, Node*);