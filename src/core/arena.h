#pragma once

#include <stddef.h>

/*
 * A region ("arena") allocator.
 *
 * Everything a compile needs only while it runs comes from one arena:
 * the AST, the words the lexer copies out of the source, and the
 * printers' working data. arena_destroy frees all of it at once. Nothing
 * is freed on its own, so code that builds the AST has no ownership to
 * track and a parse error halfway through can simply return.
 *
 * Arena memory is zeroed and aligned for any type. Running out of memory
 * aborts (see mem.h), so allocations never return NULL.
 *
 * Pointers into an arena must not outlive it. Anything handed back to the
 * compiler's caller (the result strings) is malloc'd instead.
 *
 * Built with AddressSanitizer, or with ARENA_DEBUG defined to 1, every
 * allocation gets its own malloc'd block so that ASan and Valgrind can
 * see overflows between objects.
 */

typedef struct ArenaBlock ArenaBlock;

typedef struct Arena {
  ArenaBlock* blocks;
} Arena;

/* Start an empty arena. Allocates nothing until first used. */
void arena_init(Arena*);

/* Free everything allocated from the arena. It can be used again after. */
void arena_destroy(Arena*);

/* size zeroed bytes. */
void* arena_alloc(Arena*, size_t size);

/* count * size zeroed bytes, aborting if the size overflows. */
void* arena_alloc_array(Arena*, size_t count, size_t size);

/* A copy of str. */
char* arena_strdup(Arena*, const char* str);

/* A NUL-terminated copy of at most len bytes of str (fewer if str has a
 * NUL before then). */
char* arena_strndup(Arena*, const char* str, size_t len);

#define ARENA_NEW(arena, type) ((type*)arena_alloc((arena), sizeof(type)))
