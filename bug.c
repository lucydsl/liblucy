#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct One {
  char* word;
} One;

typedef struct Two {
  char* event;
} Two;

int main() {
  printf("Hello world\n");

  char* word = "test";

  One * one = malloc(sizeof(One));
  one->word = word;

  Two * two = malloc(sizeof(Two));
  two->event = one->word;

  one->word = "another";

  printf("We got %s, %s, %s\n", word, one->word, two->event);


  return 0;
}