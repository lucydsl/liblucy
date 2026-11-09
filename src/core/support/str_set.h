#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "arena.h"

/*
 * A set of strings, for "have we seen this name" checks.
 *
 * Its table and its copies of the keys come from an arena, so there is no
 * destroy function: the set goes when the arena does. str_set_clear empties
 * it but keeps the table for reuse.
 *
 * A NULL key is never in the set, and adding one does nothing.
 */
typedef struct str_set_t {
  Arena* arena;
  const char** slots;
  size_t capacity;  // 0 or a power of two
  size_t length;
} str_set_t;

void str_set_init(str_set_t*, Arena*);
/* Add a copy of key. Returns true if it wasn't already there. */
bool str_set_add(str_set_t*, const char* key);
bool str_set_contains(const str_set_t*, const char* key);
void str_set_clear(str_set_t*);
