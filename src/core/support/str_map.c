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

#include "str_map.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

// Hash table entry (slot may be filled or empty).
typedef struct {
    const char* key;  // key is NULL if this slot is empty
    void* value;
} StrMapEntry;

// Hash table structure: create with str_map_create.
struct StrMap {
    Arena* arena;       // where entries and keys are allocated
    StrMapEntry* entries;  // hash slots
    size_t capacity;    // size of _entries array
    size_t length;      // number of items in hash table
};

#define INITIAL_CAPACITY 16  // must not be zero

StrMap* str_map_create(Arena* arena) {
    StrMap* table = ARENA_NEW(arena, StrMap);
    table->arena = arena;
    table->length = 0;
    table->capacity = INITIAL_CAPACITY;

    // Zeroed space for entry buckets.
    table->entries = arena_alloc_array(arena, table->capacity, sizeof(StrMapEntry));
    return table;
}

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

// Return 64-bit FNV-1a hash for key (NUL-terminated). See description:
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
static uint64_t hash_key(const char* key) {
    uint64_t hash = FNV_OFFSET;
    for (const char* p = key; *p; p++) {
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

void* str_map_get(StrMap* table, const char* key) {
    // AND hash with capacity-1 to ensure it's within entries array.
    uint64_t hash = hash_key(key);
    size_t index = (size_t)(hash & (uint64_t)(table->capacity - 1));

    // Loop till we find an empty entry.
    while (table->entries[index].key != NULL) {
        if (strcmp(key, table->entries[index].key) == 0) {
            // Found key, return value.
            return table->entries[index].value;
        }
        // Key wasn't in this slot, move to next (linear probing).
        index++;
        if (index >= table->capacity) {
            // At end of entries array, wrap around.
            index = 0;
        }
    }
    return NULL;
}

// Internal function to set an entry (without expanding table). A new
// key is copied into arena, unless arena is NULL (when moving entries).
static const char* set_entry(StrMapEntry* entries, size_t capacity,
        const char* key, void* value, size_t* plength, Arena* arena) {
    // AND hash with capacity-1 to ensure it's within entries array.
    uint64_t hash = hash_key(key);
    size_t index = (size_t)(hash & (uint64_t)(capacity - 1));

    // Loop till we find an empty entry.
    while (entries[index].key != NULL) {
        if (strcmp(key, entries[index].key) == 0) {
            // Found key (it already exists), update value.
            entries[index].value = value;
            return entries[index].key;
        }
        // Key wasn't in this slot, move to next (linear probing).
        index++;
        if (index >= capacity) {
            // At end of entries array, wrap around.
            index = 0;
        }
    }

    // Didn't find key, allocate+copy if needed, then insert it.
    if (arena != NULL) {
        key = arena_strdup(arena, key);
    }
    if (plength != NULL) {
        (*plength)++;
    }
    entries[index].key = (char*)key;
    entries[index].value = value;
    return key;
}

// Expand hash table to twice its current size.
static void expand(StrMap* table) {
    // Allocate new entries array (zeroed, size overflow checked).
    size_t new_capacity = size_mul(table->capacity, 2);
    StrMapEntry* new_entries = arena_alloc_array(table->arena, new_capacity,
                                              sizeof(StrMapEntry));

    // Iterate entries, move all non-empty ones to new table's entries.
    for (size_t i = 0; i < table->capacity; i++) {
        StrMapEntry entry = table->entries[i];
        if (entry.key != NULL) {
            set_entry(new_entries, new_capacity, entry.key,
                         entry.value, NULL, NULL);
        }
    }

    // The old entries array stays in the arena until it is destroyed.
    table->entries = new_entries;
    table->capacity = new_capacity;
}

const char* str_map_set(StrMap* table, const char* key, void* value) {
    assert(value != NULL);
    if (value == NULL) {
        return NULL;
    }

    // If length will exceed half of current capacity, expand it.
    if (table->length >= table->capacity / 2) {
        expand(table);
    }

    // Set entry and update length.
    return set_entry(table->entries, table->capacity, key, value,
                        &table->length, table->arena);
}

StrMapIterator str_map_iterator(StrMap* table) {
    StrMapIterator it;
    it.table = table;
    it.index = 0;
    return it;
}

bool str_map_next(StrMapIterator* it) {
    // Loop till we've hit end of entries array.
    StrMap* table = it->table;
    while (it->index < table->capacity) {
        size_t i = it->index;
        it->index++;
        if (table->entries[i].key != NULL) {
            // Found next non-empty item, update iterator key and value.
            StrMapEntry entry = table->entries[i];
            it->key = entry.key;
            it->value = entry.value;
            return true;
        }
    }
    return false;
}