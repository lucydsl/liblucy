#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "string_list.h"

void string_list_init(string_list_t* list) {
  list->head = NULL;
  list->tail = NULL;
  list->length = 0;
}

void string_list_destroy(string_list_t* list) {
  string_list_node_t* node = list->head;
  while(node != NULL) {
    string_list_node_t* next = node->next;
    if(node->value != NULL) {
      free(node->value);
    }
    free(node);
    node = next; 
  }
}

void string_list_append(string_list_t* list, char* value) {
  string_list_node_t *node = malloc(sizeof(*node));
  node->next = NULL;
  node->value = strdup(value);

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
    return true;
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