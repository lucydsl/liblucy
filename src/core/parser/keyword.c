#include "internal.h"

#include <stddef.h>
#include <string.h>

static const struct {
  const char* name;
  Keyword key;
} keywords[] = {
  { "use", KEYWORD_USE },
  { "state", KEYWORD_STATE },
  { "initial", KEYWORD_INITIAL },
  { "final", KEYWORD_FINAL },
  { "action", KEYWORD_ACTION },
  { "guard", KEYWORD_GUARD },
  { "assign", KEYWORD_ASSIGN },
  { "invoke", KEYWORD_INVOKE },
  { "machine", KEYWORD_MACHINE },
  { "delay", KEYWORD_DELAY },
  { "on", KEYWORD_ON },
  { "spawn", KEYWORD_SPAWN },
  { "send", KEYWORD_SEND },
  { "event", KEYWORD_EVENT },
};

Keyword keyword_lookup(const char* key) {
  if(key == NULL) {
    return KEYWORD_NONE;
  }
  for(size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    if(strcmp(keywords[i].name, key) == 0) {
      return keywords[i].key;
    }
  }
  return KEYWORD_NONE;
}

LocalKind local_lookup(const char* key) {
  if(key == NULL) {
    return LOCAL_NONE;
  }
  if(strcmp(key, "@entry") == 0) {
    return LOCAL_ENTRY;
  }
  if(strcmp(key, "@exit") == 0) {
    return LOCAL_EXIT;
  }
  return LOCAL_NONE;
}

const char* local_name(LocalKind key) {
  if(key == LOCAL_ENTRY) {
    return "@entry";
  } else if(key == LOCAL_EXIT) {
    return "@exit";
  }
  // Should never happen.
  return NULL;
}
