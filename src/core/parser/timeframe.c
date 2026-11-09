#include "internal.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../support/str_builder.h"

typedef enum TimeUnit {
  TF_MS,
  TF_S,
  TF_M,
} TimeUnit;

static const struct {
  const char* name;
  TimeUnit key;
} suffixes[] = {
  { "ms", TF_MS },
  { "s", TF_S },
  { "m", TF_M },
};

// The TF_ value for a suffix, or -1 if it isn't one.
static int timeframe_get(const char* key) {
  for(size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
    if(strcmp(suffixes[i].name, key) == 0) {
      return suffixes[i].key;
    }
  }
  return -1;
}

bool timeframe_is_digit(char c) {
  return c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' ||
    c == '6' || c == '7' || c == '8' || c == '9';
}

bool timeframe_is_char(char c) {
  return timeframe_is_digit(c) || c == 'm' || c == 's';
}

bool timeframe_is_suffix(const char* key) {
  // TF_MS is 0, which the dict this used to look suffixes up in couldn't
  // tell apart from a missing key, so "ms" has always been rejected as an
  // unknown suffix. Kept that way here; changing it changes behaviour.
  return timeframe_get(key) > 0;
}

// The digits as a number, or -1 if it is bigger than INT_MAX. (atoi, used
// before, is undefined for numbers that don't fit.)
static long long parse_digits(const char* digits) {
  long long value = 0;
  for(const char* p = digits; *p; p++) {
    value = value * 10 + (*p - '0');
    if(value > INT_MAX) {
      return -1;
    }
  }
  return value;
}

static Timeframe timeframe_error(const char* message) {
  Timeframe tf = {
    .timeframe_is_digit = false,
    .time = 0,
    .error = (char*)message
  };
  return tf;
}

Timeframe timeframe_parse(char* word, size_t word_len) {
  StrBuilder* int_sb = str_builder_create();
  StrBuilder* tf_sb = str_builder_create();
  size_t int_len = 0;
  size_t tf_len = 0;
  bool is_int = true;

  size_t i = 0;
  char c;
  while(i < word_len) {
    c = word[i];
    if(timeframe_is_digit(c)) {
      str_builder_add_char(int_sb, c);
      int_len++;
    } else {
      str_builder_add_char(tf_sb, c);
      tf_len++;
      is_int = false;
    }
    i++;
  }

  char* int_str = str_builder_dump(int_sb, &int_len);
  char* tf_str = str_builder_dump(tf_sb, &tf_len);

  long long value = parse_digits(int_str);
  if(!is_int) {
    if(!timeframe_is_suffix(tf_str)) {
      Timeframe tf = timeframe_error("Unknown timeframe suffix");
      free(int_str);
      free(tf_str);
      str_builder_destroy(tf_sb);
      str_builder_destroy(int_sb);
      return tf;
    }
    
    int key = timeframe_get(tf_str);
    switch(key) {
      case TF_MS: {
        // 2ms = 2
        break;
      }
      case TF_S: {
        // 2s = 2000
        value = value * 1000;
        break;
      }
      case TF_M: {
        // 2m = 120000
        value = value * 60000;
        break;
      }
    }
  }

  free(int_str);
  free(tf_str);
  str_builder_destroy(tf_sb);
  str_builder_destroy(int_sb);

  // Delays are printed as ints, so anything bigger can't be represented.
  // The multiplication used to overflow int.
  if(value < 0 || value > INT_MAX) {
    return timeframe_error("Delay is too long. The longest delay is 2147483647ms.");
  }

  Timeframe tf = {
    .timeframe_is_digit = is_int,
    .time = (int)value,
    .error = NULL
  };

  return tf;
}
