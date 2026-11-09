#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "arena.h"

/*
 * An append-only list of strings. Its nodes and its copies of the values
 * come from an arena, so there is no destroy function: the list goes when
 * the arena does.
 */

typedef struct string_list_node_t {
  char* value;
  struct string_list_node_t* next;
} string_list_node_t;

typedef struct string_list_t {
  Arena* arena;
  string_list_node_t* head;
  string_list_node_t* tail;
  size_t length;
} string_list_t;

typedef struct string_list_iterator_t {
  string_list_node_t* node;
  size_t index;

  string_list_t* _list;
} string_list_iterator_t;

void string_list_init(string_list_t*, Arena*);
/* Append a copy of value. */
void string_list_append(string_list_t*, const char*);
string_list_iterator_t string_list_iterator(string_list_t*);
bool string_list_next(string_list_iterator_t*);

static inline bool string_list_empty(string_list_t* list) {
  return list->head == NULL;
}