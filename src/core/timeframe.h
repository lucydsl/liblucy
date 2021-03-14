#pragma once

#include <stdbool.h>

typedef struct Timeframe {
  bool is_integer;
  int time;
  char* error;
} Timeframe; 

bool is_integer(char);
bool is_timeframe_char(char);
bool timeframe_is_suffix(char*);
Timeframe timeframe_parse(char*, size_t);
void timeframe_init();