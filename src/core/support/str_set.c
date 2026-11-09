#include "str_set.h"

#include <stdint.h>
#include <string.h>

#include "arena.h"
#include "mem.h"

#define STR_SET_INITIAL_CAPACITY 16

// 64-bit FNV-1a.
static uint64_t hash_key(const char* key) {
  uint64_t hash = 14695981039346656037ULL;
  for(const char* p = key; *p; p++) {
    hash ^= (uint64_t)(unsigned char)*p;
    hash *= 1099511628211ULL;
  }
  return hash;
}

// The slot holding key, or the empty slot where it would go. The table is
// never more than half full, so there always is one.
static size_t find_slot(const char** slots, size_t capacity, const char* key) {
  size_t mask = capacity - 1;
  size_t index = (size_t)(hash_key(key) & mask);
  while(slots[index] != NULL && strcmp(slots[index], key) != 0) {
    index = (index + 1) & mask;
  }
  return index;
}

static void grow(StrSet* set) {
  size_t capacity = set->capacity ? size_mul(set->capacity, 2) : STR_SET_INITIAL_CAPACITY;
  const char** slots = arena_alloc_array(set->arena, capacity, sizeof(*slots));
  for(size_t i = 0; i < set->capacity; i++) {
    if(set->slots[i] != NULL) {
      slots[find_slot(slots, capacity, set->slots[i])] = set->slots[i];
    }
  }
  // The old table stays in the arena until it is destroyed.
  set->slots = slots;
  set->capacity = capacity;
}

void str_set_init(StrSet* set, Arena* arena) {
  set->arena = arena;
  set->slots = NULL;
  set->capacity = 0;
  set->length = 0;
}

bool str_set_contains(const StrSet* set, const char* key) {
  if(key == NULL || set->length == 0) {
    return false;
  }
  return set->slots[find_slot(set->slots, set->capacity, key)] != NULL;
}

bool str_set_add(StrSet* set, const char* key) {
  if(key == NULL || str_set_contains(set, key)) {
    return false;
  }
  if(size_mul(size_add(set->length, 1), 2) > set->capacity) {
    grow(set);
  }
  set->slots[find_slot(set->slots, set->capacity, key)] = arena_strdup(set->arena, key);
  set->length++;
  return true;
}

void str_set_clear(StrSet* set) {
  if(set->slots != NULL) {
    memset(set->slots, 0, set->capacity * sizeof(*set->slots));
  }
  set->length = 0;
}
