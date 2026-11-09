#include <stddef.h>
#include <string.h>
#include "local_kind.h"

unsigned short local_get(const char* key) {
  if(key == NULL) {
    return 0;
  }
  if(strcmp(key, "@entry") == 0) {
    return LOCAL_ENTRY;
  }
  if(strcmp(key, "@exit") == 0) {
    return LOCAL_EXIT;
  }
  return 0;
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
