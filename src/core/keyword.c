#include <stdlib.h>
#include <string.h>
#include "dict.h"
#include "keyword.h"

dict *keywords;

bool is_keyword(char* key) {
  return dict_search(keywords, key) > 0;
}

unsigned short keyword_get(char* key) {
  return dict_search(keywords, key);
}

void keyword_init() {
  keywords = dict_create();

  dict_insert(keywords, "import", KW_IMPORT);
  dict_insert(keywords, "state", KW_STATE);
  dict_insert(keywords, "initial", KW_INITIAL);
  dict_insert(keywords, "final", KW_FINAL);
  dict_insert(keywords, "action", KW_ACTION);
  dict_insert(keywords, "guard", KW_GUARD);
  dict_insert(keywords, "assign", KW_ASSIGN);
  dict_insert(keywords, "invoke", KW_INVOKE);
  dict_insert(keywords, "machine", KW_MACHINE);
  dict_insert(keywords, "delay", KW_DELAY);
}