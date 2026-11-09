#include "mem.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mem_fail(const char* what) {
  fprintf(stderr, "liblucy: %s\n", what);
  abort();
}

void* xmalloc(size_t size) {
  // malloc(0) may return NULL, which would look like a failure.
  void* ptr = malloc(size ? size : 1);
  if(ptr == NULL) {
    mem_fail("out of memory");
  }
  return ptr;
}

void* xcalloc(size_t count, size_t size) {
  // calloc checks count * size for overflow itself.
  void* ptr = calloc(count ? count : 1, size ? size : 1);
  if(ptr == NULL) {
    mem_fail("out of memory");
  }
  return ptr;
}

void* xrealloc(void* ptr, size_t size) {
  void* out = realloc(ptr, size ? size : 1);
  if(out == NULL) {
    mem_fail("out of memory");
  }
  return out;
}

char* xstrdup(const char* str) {
  size_t len = strlen(str);
  char* out = xmalloc(size_add(len, 1));
  memcpy(out, str, len + 1);
  return out;
}

size_t size_add(size_t a, size_t b) {
  if(a > SIZE_MAX - b) {
    mem_fail("allocation size overflow");
  }
  return a + b;
}

size_t size_mul(size_t a, size_t b) {
  if(b != 0 && a > SIZE_MAX / b) {
    mem_fail("allocation size overflow");
  }
  return a * b;
}
