#include "arena.h"

#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

#ifndef ARENA_DEBUG
# if defined(__SANITIZE_ADDRESS__)
#  define ARENA_DEBUG 1
# elif defined(__has_feature)
#  if __has_feature(address_sanitizer)
#   define ARENA_DEBUG 1
#  endif
# endif
#endif
#ifndef ARENA_DEBUG
# define ARENA_DEBUG 0
#endif

// Usual block size. Bigger allocations get a block to themselves.
#define ARENA_BLOCK_SIZE (16 * 1024)
#define ARENA_BIG_ALLOC (ARENA_BLOCK_SIZE / 4)

struct ArenaBlock {
  ArenaBlock* next;
  size_t used;
  size_t size;
  // The memory handed out, aligned for any type.
  max_align_t data[];
};

#if !ARENA_DEBUG
static size_t align_up(size_t size) {
  size_t align = alignof(max_align_t);
  return size_add(size, align - 1) & ~(align - 1);
}
#endif

static ArenaBlock* block_create(size_t size) {
  ArenaBlock* block = xcalloc(1, size_add(sizeof(ArenaBlock), size));
  block->size = size;
  return block;
}

void arena_init(Arena* arena) {
  arena->blocks = NULL;
}

void arena_destroy(Arena* arena) {
  ArenaBlock* block = arena->blocks;
  while(block != NULL) {
    ArenaBlock* next = block->next;
    free(block);
    block = next;
  }
  arena->blocks = NULL;
}

// Allocate size bytes in a block of their own, kept behind the head so
// that the head's free space can still be used.
static void* alloc_own_block(Arena* arena, size_t size) {
  ArenaBlock* block = block_create(size);
  block->used = size;
  ArenaBlock* head = arena->blocks;
  if(head != NULL) {
    block->next = head->next;
    head->next = block;
  } else {
    arena->blocks = block;
  }
  return block->data;
}

void* arena_alloc(Arena* arena, size_t size) {
  if(size == 0) {
    size = 1;
  }

#if ARENA_DEBUG
  // Exactly size bytes, so ASan sees an overflow of even one byte.
  return alloc_own_block(arena, size);
#else
  size = align_up(size);
  ArenaBlock* head = arena->blocks;

  if(head != NULL && head->size - head->used >= size) {
    void* ptr = (char*)head->data + head->used;
    head->used += size;
    return ptr;
  }

  if(size > ARENA_BIG_ALLOC) {
    return alloc_own_block(arena, size);
  }

  ArenaBlock* block = block_create(ARENA_BLOCK_SIZE);
  block->used = size;
  block->next = head;
  arena->blocks = block;
  return block->data;
#endif
}

void* arena_alloc_array(Arena* arena, size_t count, size_t size) {
  return arena_alloc(arena, size_mul(count, size));
}

char* arena_strndup(Arena* arena, const char* str, size_t len) {
  const char* nul = memchr(str, '\0', len);
  if(nul != NULL) {
    len = (size_t)(nul - str);
  }
  char* out = arena_alloc(arena, size_add(len, 1));
  memcpy(out, str, len);
  // Already NUL-terminated: arena memory is zeroed.
  return out;
}

char* arena_strdup(Arena* arena, const char* str) {
  return arena_strndup(arena, str, strlen(str));
}
