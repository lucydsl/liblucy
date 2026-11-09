#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "arena.h"

/*
 * An append-only list of strings. Its nodes and its copies of the values
 * come from an arena, so there is no destroy function: the list goes when
 * the arena does.
 */

typedef struct StringListNode {
  const char* value;
  struct StringListNode* next;
} StringListNode;

typedef struct StringList {
  Arena* arena;
  StringListNode* head;
  StringListNode* tail;
  size_t length;
} StringList;

typedef struct StringListIterator {
  StringListNode* node;
  size_t index;

  StringList* list;
} StringListIterator;

void string_list_init(StringList*, Arena*);
/* Append a copy of value. */
void string_list_append(StringList*, const char*);
StringListIterator string_list_iterator(StringList*);
bool string_list_next(StringListIterator*);