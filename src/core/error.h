#pragma once

#include "node.h"
#include "state.h"

void error_file_info(State*);
void error_annotate(State*, Node*);
void error_message(char*);
void error_msg_with_code_block(State*, Node*, const char*);
void error_unexpected_identifier(State*, Node*);