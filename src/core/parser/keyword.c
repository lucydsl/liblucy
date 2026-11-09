#include <stddef.h>
#include <string.h>
#include "keyword.h"

static const struct {
  const char* name;
  unsigned short key;
} keywords[] = {
  { "use", KW_USE },
  { "state", KW_STATE },
  { "initial", KW_INITIAL },
  { "final", KW_FINAL },
  { "action", KW_ACTION },
  { "guard", KW_GUARD },
  { "assign", KW_ASSIGN },
  { "invoke", KW_INVOKE },
  { "machine", KW_MACHINE },
  { "delay", KW_DELAY },
  { "on", KW_ON },
  { "spawn", KW_SPAWN },
  { "send", KW_SEND },
};

bool is_keyword(const char* key) {
  return keyword_get(key) != KW_NOT_A_KEYWORD;
}

unsigned short keyword_get(const char* key) {
  if(key == NULL) {
    return KW_NOT_A_KEYWORD;
  }
  for(size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    if(strcmp(keywords[i].name, key) == 0) {
      return keywords[i].key;
    }
  }
  return KW_NOT_A_KEYWORD;
}
