#include "string_list.h"

#include <stddef.h>

#include "arena.h"

void string_list_init(StringList* list, Arena* arena) {
  list->arena = arena;
  list->head = NULL;
  list->tail = NULL;
  list->length = 0;
}

void string_list_append(StringList* list, const char* value) {
  StringListNode *node = ARENA_NEW(list->arena, StringListNode);
  node->value = arena_strdup(list->arena, value);

  if(list->tail != NULL) {
    list->tail->next = node;
    list->tail = node;
  } else {
    list->head = node;
    list->tail = node;
  }
  list->length++;
}

StringListIterator string_list_iterator(StringList* list) {
  StringListIterator iterator;
  iterator.list = list;
  iterator.index = 0;
  iterator.node = NULL;
  return iterator;
}

bool string_list_next(StringListIterator* iterator) {
  if(iterator->node == NULL) {
    iterator->node = iterator->list->head;
    return iterator->node != NULL;
  } else {
    StringListNode* node = iterator->node->next;
    if(node == NULL) {
      return false;
    }
    iterator->node = node;
    iterator->index++;
    return true;
  }
}
