#include <stddef.h>
#include "arena.h"
#include "string_list.h"

void string_list_init(string_list_t* list, Arena* arena) {
  list->arena = arena;
  list->head = NULL;
  list->tail = NULL;
  list->length = 0;
}

void string_list_append(string_list_t* list, const char* value) {
  string_list_node_t *node = ARENA_NEW(list->arena, string_list_node_t);
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

string_list_iterator_t string_list_iterator(string_list_t* list) {
  string_list_iterator_t iterator;
  iterator._list = list;
  iterator.index = 0;
  iterator.node = NULL;
  return iterator;
}

bool string_list_next(string_list_iterator_t* iterator) {
  if(iterator->node == NULL) {
    iterator->node = iterator->_list->head;
    return iterator->node != NULL;
  } else {
    string_list_node_t* node = iterator->node->next;
    if(node == NULL) {
      return false;
    }
    iterator->node = node;
    iterator->index++;
    return true;
  }
}
