#pragma once

#include <stddef.h>

/*
 * Checked allocation.
 *
 * None of these return NULL. A compiler can't do anything useful after
 * running out of memory, so they print a message and abort() instead,
 * which saves every caller from checking. The array and size helpers also
 * abort if a size calculation would overflow size_t, so a huge count can't
 * wrap around into a small allocation.
 *
 * Memory from these is freed with free().
 */

void* xmalloc(size_t size);
void* xcalloc(size_t count, size_t size);
void* xrealloc(void* ptr, size_t size);
char* xstrdup(const char* str);

/* a + b and a * b, aborting on overflow. */
size_t size_add(size_t a, size_t b);
size_t size_mul(size_t a, size_t b);

/* Report an impossible allocation and abort. */
void mem_fail(const char* what);
