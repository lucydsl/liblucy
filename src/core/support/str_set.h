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
typedef struct StrSet {
  Arena* arena;
  const char** slots;
  size_t capacity; // 0 or a power of two
  size_t length;
} StrSet;

void str_set_init(StrSet*, Arena*);
/* Add a copy of key. Returns true if it wasn't already there. */
bool str_set_add(StrSet*, const char* key);
bool str_set_contains(const StrSet*, const char* key);
void str_set_clear(StrSet*);
