#pragma once

#include <stdbool.h>

#define KW_USE 1
#define KW_STATE 2
#define KW_INITIAL 3
#define KW_FINAL 4
#define KW_ACTION 5
#define KW_GUARD 6
#define KW_ASSIGN 7
#define KW_INVOKE 8
#define KW_MACHINE 9
#define KW_DELAY 10
#define KW_ON 11
#define KW_SPAWN 12
#define KW_SEND 13

bool is_keyword(char*);
unsigned short keyword_get(char*);
void keyword_init();