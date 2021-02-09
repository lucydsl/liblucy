#include <stdio.h> // can remove
#include <stdlib.h>
#include "set.h"

SimpleSet *valid_chars;
char str[2] = {0};

// This is silly, use Set instead
int is_valid_identifier_char(char c) {
  str[0] = c;
  return set_contains(valid_chars, str) == SET_TRUE;
}

void identifier_init() {
  valid_chars = malloc(sizeof *valid_chars);
  set_init(valid_chars);
  set_add(valid_chars, "a");
  set_add(valid_chars, "b");
  set_add(valid_chars, "c");
  set_add(valid_chars, "d");
  set_add(valid_chars, "e");
  set_add(valid_chars, "f");
  set_add(valid_chars, "g");
  set_add(valid_chars, "h");
  set_add(valid_chars, "i");
  set_add(valid_chars, "j");
  set_add(valid_chars, "k");
  set_add(valid_chars, "l");
  set_add(valid_chars, "m");
  set_add(valid_chars, "n");
  set_add(valid_chars, "o");
  set_add(valid_chars, "p");
  set_add(valid_chars, "q");
  set_add(valid_chars, "r");
  set_add(valid_chars, "s");
  set_add(valid_chars, "t");
  set_add(valid_chars, "u");
  set_add(valid_chars, "v");
  set_add(valid_chars, "w");
  set_add(valid_chars, "x");
  set_add(valid_chars, "y");
  set_add(valid_chars, "z");
  set_add(valid_chars, "A");
  set_add(valid_chars, "B");
  set_add(valid_chars, "C");
  set_add(valid_chars, "D");
  set_add(valid_chars, "E");
  set_add(valid_chars, "F");
  set_add(valid_chars, "G");
  set_add(valid_chars, "H");
  set_add(valid_chars, "I");
  set_add(valid_chars, "J");
  set_add(valid_chars, "K");
  set_add(valid_chars, "L");
  set_add(valid_chars, "M");
  set_add(valid_chars, "N");
  set_add(valid_chars, "O");
  set_add(valid_chars, "P");
  set_add(valid_chars, "Q");
  set_add(valid_chars, "R");
  set_add(valid_chars, "S");
  set_add(valid_chars, "T");
  set_add(valid_chars, "U");
  set_add(valid_chars, "V");
  set_add(valid_chars, "W");
  set_add(valid_chars, "X");
  set_add(valid_chars, "Y");
  set_add(valid_chars, "Z");

}