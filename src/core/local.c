#include <stdlib.h>
#include <string.h>
#include "dict.h"
#include "local.h"

static dict *locals;

unsigned short local_get(char* key) {
  return dict_search(locals, key);
}

const char* local_get_name(unsigned short key) {
  if(key == LOCAL_ENTRY) {
    return "@entry";
  } else if(key == LOCAL_EXIT) {
    return "@exit";
  }
  // Should never happen.
  return NULL;
}

void local_init() {
  locals = dict_create();

  dict_insert(locals, "@entry", LOCAL_ENTRY);
  dict_insert(locals, "@exit", LOCAL_EXIT);
}