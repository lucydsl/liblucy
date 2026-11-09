#include "internal.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

bool timeframe_is_digit(char c) {
  return c >= '0' && c <= '9';
}

bool timeframe_is_char(char c) {
  return timeframe_is_digit(c) || c == 'm' || c == 's';
}

static Timeframe timeframe_error(const char* message) {
  Timeframe tf = {
    .time = 0,
    .error = message
  };
  return tf;
}

Timeframe timeframe_parse(const char* word, size_t len) {
  // The digits make the number and everything else the suffix, wherever
  // they are, so "1s2" is 12s.
  long long value = 0;
  bool too_long = false;
  size_t suffix_len = 0;
  char suffix = '\0';
  for(size_t i = 0; i < len; i++) {
    char c = word[i];
    if(timeframe_is_digit(c)) {
      if(!too_long) {
        value = value * 10 + (c - '0');
        too_long = value > INT_MAX;
      }
    } else {
      suffix = c;
      suffix_len++;
    }
  }

  if(suffix_len > 0) {
    // Known bug: "ms" isn't accepted, though the error message suggests
    // it. Only "s" and "m" are.
    if(suffix_len != 1 || (suffix != 's' && suffix != 'm')) {
      return timeframe_error("Unknown timeframe suffix");
    }
    if(!too_long) {
      value *= suffix == 's' ? 1000 : 60000;
      too_long = value > INT_MAX;
    }
  }

  // Delays are printed as ints, so anything bigger can't be represented.
  if(too_long) {
    return timeframe_error("Delay is too long. The longest delay is 2147483647ms.");
  }

  Timeframe tf = {
    .time = (int)value,
    .error = NULL
  };
  return tf;
}
