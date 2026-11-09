// Compiles every file it is given, several rounds over, in one process,
// with each combination of the remote-imports and .d.ts options, and
// checks that every round gives the same result as the first.
//
// One process stands in for a long-lived wasm instance: `make
// test-sanitize` builds this with AddressSanitizer, UndefinedBehavior-
// Sanitizer and leak detection, so memory errors, leaks and state that
// carries over from one compile to the next all fail the run. The snapshot
// tests can't show the last of these, as they start a new process each.
//
// Usage: compile_all [--rounds N] [--generated] file...
//
// --generated adds inputs too big to keep as files; see add_generated.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/core/lucy.h"

#define OPTION_SETS 4

typedef struct Source {
  const char* name;
  char* text;
} Source;

typedef struct Output {
  bool success;
  char* js;
  char* dts;
} Output;

static char* read_file(const char* path) {
  FILE* fp = fopen(path, "rb");
  if(fp == NULL) {
    return NULL;
  }
  char* buffer = NULL;
  long length = -1;
  if(fseek(fp, 0, SEEK_END) == 0) {
    length = ftell(fp);
  }
  if(length >= 0 && fseek(fp, 0, SEEK_SET) == 0) {
    buffer = malloc((size_t)length + 1);
    if(buffer != NULL) {
      size_t got = fread(buffer, 1, (size_t)length, fp);
      buffer[got] = '\0';
    }
  }
  fclose(fp);
  return buffer;
}

static char* copy(const char* str) {
  if(str == NULL) {
    return NULL;
  }
  size_t len = strlen(str);
  char* out = malloc(len + 1);
  if(out == NULL) {
    abort();
  }
  memcpy(out, str, len + 1);
  return out;
}

// A growable string for building the generated inputs.
typedef struct Text {
  char* data;
  size_t len;
  size_t cap;
} Text;

static void text_add(Text* text, const char* str, size_t count) {
  size_t len = strlen(str);
  for(size_t i = 0; i < count; i++) {
    if(text->len + len + 1 > text->cap) {
      text->cap = (text->len + len + 1) * 2;
      text->data = realloc(text->data, text->cap);
      if(text->data == NULL) {
        abort();
      }
    }
    memcpy(text->data + text->len, str, len + 1);
    text->len += len;
  }
}

static char* repeat(char c, size_t count) {
  char* str = malloc(count + 1);
  if(str == NULL) {
    abort();
  }
  memset(str, c, count);
  str[count] = '\0';
  return str;
}

static char* nested_machines(size_t depth) {
  Text text = {0};
  text_add(&text, "machine m {\n  state a {\n", depth);
  text_add(&text, "    go => b\n    @entry => action(:x)\n", 1);
  text_add(&text, "  }\n}\n", depth);
  return text.data;
}

// Identifiers longer than 65535 characters, which overflowed the unsigned
// shorts some lengths and positions were kept in; the deepest nesting of
// machines allowed and one level more; and a machine with 2000 states.
static int add_generated(Source* sources) {
  char* a = repeat('a', 100000);
  char* b = repeat('b', 100000);
  Text text = {0};
  text_add(&text, "state ", 1);
  text_add(&text, a, 1);
  text_add(&text, " {\n  ", 1);
  text_add(&text, b, 1);
  text_add(&text, " => ", 1);
  text_add(&text, a, 1);
  text_add(&text, "\n  @entry => action(:", 1);
  text_add(&text, b, 1);
  text_add(&text, ")\n  delay(:", 1);
  text_add(&text, a, 1);
  text_add(&text, ") => assign(", 1);
  text_add(&text, b, 1);
  text_add(&text, ", :", 1);
  text_add(&text, a, 1);
  text_add(&text, ") => ", 1);
  text_add(&text, b, 1);
  text_add(&text, "\n}\n", 1);
  free(a);
  free(b);
  sources[0] = (Source){ "<generated: long identifiers>", text.data };

  sources[1] = (Source){ "<generated: 64 nested machines>", nested_machines(64) };
  sources[2] = (Source){ "<generated: 65 nested machines>", nested_machines(65) };

  Text states = {0};
  text_add(&states, "machine m {\n", 1);
  for(int i = 0; i < 2000; i++) {
    char line[64];
    snprintf(line, sizeof(line), "  state s%c%c%c {\n    go => a\n  }\n",
      'a' + i % 26, 'a' + i / 26 % 26, 'a' + i / 676 % 26);
    text_add(&states, line, 1);
  }
  text_add(&states, "}\n", 1);
  sources[3] = (Source){ "<generated: 2000 states>", states.data };
  return 4;
}

#define GENERATED_COUNT 4

static bool same(const char* a, const char* b) {
  if(a == NULL || b == NULL) {
    return a == b;
  }
  return strcmp(a, b) == 0;
}

int main(int argc, char** argv) {
  int rounds = 3;
  bool generated = false;
  int first = 1;
  while(first < argc && strncmp(argv[first], "--", 2) == 0) {
    if(strcmp(argv[first], "--rounds") == 0 && first + 1 < argc) {
      rounds = atoi(argv[first + 1]);
      first += 2;
    } else if(strcmp(argv[first], "--generated") == 0) {
      generated = true;
      first++;
    } else {
      rounds = 0;
      break;
    }
  }
  if(rounds < 1 || (first >= argc && !generated)) {
    fprintf(stderr, "Usage: %s [--rounds N] [--generated] file...\n", argv[0]);
    return 2;
  }

  int count = argc - first + (generated ? GENERATED_COUNT : 0);
  Source* sources = calloc((size_t)count, sizeof(*sources));
  Output* outputs = calloc((size_t)count * OPTION_SETS, sizeof(*outputs));
  if(sources == NULL || outputs == NULL) {
    abort();
  }

  int failed = 0;
  int loaded = 0;
  for(int arg = first; arg < argc; arg++, loaded++) {
    sources[loaded].name = argv[arg];
    sources[loaded].text = read_file(argv[arg]);
    if(sources[loaded].text == NULL) {
      fprintf(stderr, "Can't read %s\n", argv[arg]);
      failed = 1;
    }
  }
  if(generated) {
    add_generated(sources + loaded);
  }

  // One result is reused for every compile of the last round, as a caller
  // may do; the earlier rounds create and destroy one per compile.
  LucyResult* reused = lucy_result_create();

  for(int round = 0; round < rounds; round++) {
    bool reuse = round == rounds - 1 && rounds > 1;
    for(int i = 0; i < count; i++) {
      if(sources[i].text == NULL) {
        continue;
      }
      const char* filename = sources[i].name;
      for(int set = 0; set < OPTION_SETS; set++) {
        bool use_remote = set & 1;
        bool include_dts = set & 2;
        LucyResult* result = reuse ? reused : lucy_result_create();
        lucy_result_set_options(result, use_remote, include_dts);
        lucy_compile_xstate(result, sources[i].text, filename);

        Output* expected = &outputs[i * OPTION_SETS + set];
        if(round == 0) {
          expected->success = lucy_result_success(result);
          expected->js = copy(lucy_result_js(result));
          expected->dts = copy(lucy_result_dts(result));
        } else if(lucy_result_success(result) != expected->success ||
          !same(lucy_result_js(result), expected->js) || !same(lucy_result_dts(result), expected->dts)) {
          printf("FAILED - %s (remote %d, dts %d) changed in round %d\n",
            filename, use_remote, include_dts, round + 1);
          failed = 1;
        }

        if(!reuse) {
          lucy_result_destroy(result);
        }
      }
    }
  }

  lucy_result_destroy(reused);
  for(int i = 0; i < count * OPTION_SETS; i++) {
    free(outputs[i].js);
    free(outputs[i].dts);
  }
  for(int i = 0; i < count; i++) {
    free(sources[i].text);
  }
  free(outputs);
  free(sources);
  return failed;
}
