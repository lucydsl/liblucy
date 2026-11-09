/**
MIT License

Copyright (c) 2021 Ben Hoyt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
 
// Simple hash table implemented in C.
//
// Changed for liblucy: the table, its entries and its copies of the keys
// are allocated from an arena (see arena.h), so there is no str_map_destroy;
// the table goes when the arena does. Values are the caller's to manage.
// Hashing, probing and growth are unchanged, so iteration order is too.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "arena.h"

// Hash table structure: create with str_map_create.
typedef struct StrMap StrMap;

// Create a hash table in arena. Never returns NULL.
StrMap* str_map_create(Arena* arena);

// Get item with given key (NUL-terminated) from hash table. Return
// value (which was set with str_map_set), or NULL if key not found.
void* str_map_get(StrMap* table, const char* key);

// Set item with given key (NUL-terminated) to value (which must not
// be NULL). If not already present in table, key is copied into the
// arena. Return address of copied key.
const char* str_map_set(StrMap* table, const char* key, void* value);

// Return number of items in hash table.
size_t str_map_length(StrMap* table);

// Hash table iterator: create with str_map_iterator, iterate with str_map_next.
typedef struct {
    const char* key;  // current key
    void* value;      // current value

    // Don't use these fields directly.
    StrMap* table;       // reference to hash table being iterated
    size_t index;    // current index into the table's entries
} StrMapIterator;

// Return new hash table iterator (for use with str_map_next).
StrMapIterator str_map_iterator(StrMap* table);

// Move iterator to next item in hash table, update iterator's key
// and value to current item, and return true. If there are no more
// items, return false. Don't call str_map_set during iteration.
bool str_map_next(StrMapIterator* it);
