#pragma once

#include "lexer.h"

int parser_consume_machine(State*);
int parser_consume_implicit_machine(State*, int);