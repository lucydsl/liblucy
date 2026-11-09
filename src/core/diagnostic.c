#include "diagnostic.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "support/mem.h"
#include "support/str_builder.h"

#define RESET     "\033[0m"
#define RED       "\033[31m"
#define BOLDRED   "\033[1m\033[31m"
#define BOLDWHITE "\033[1m\033[37m"

#define CODE_BLOCK_INDENT 4

void diagnostic_list_init(DiagnosticList* list) {
  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

void diagnostic_list_clear(DiagnosticList* list) {
  for(size_t i = 0; i < list->count; i++) {
    free(list->items[i].message);
    free(list->items[i].text);
  }
  free(list->items);
  diagnostic_list_init(list);
}

// Take ownership of message and text, which must be malloc'd.
static void add(DiagnosticList* list, LucySeverity severity, LucyStream stream,
  char* message, long line, long column, char* text) {
  if(list->count == list->capacity) {
    list->capacity = list->capacity ? size_mul(list->capacity, 2) : 4;
    list->items = xrealloc(list->items, size_mul(list->capacity, sizeof(*list->items)));
  }
  list->items[list->count++] = (LucyDiagnostic){
    .severity = severity,
    .stream = stream,
    .message = message,
    .line = line,
    .column = column,
    .text = text
  };
}

static int num_places(long n) {
  unsigned long u = n < 0 ? -(unsigned long)n : (unsigned long)n;
  int places = 1;
  while(u >= 10) {
    u /= 10;
    places++;
  }
  return places;
}

static void add_spaces(StrBuilder* sb, long count) {
  for(long i = 0; i < count; i++) {
    str_builder_add_char(sb, ' ');
  }
}

static void add_code_line(StrBuilder* sb, StrBuilder* code, long line, int max_spaces) {
  int line_spaces = num_places(line);
  int num_spaces = max_spaces - line_spaces + 1;

  str_builder_add_format(sb, BOLDWHITE "    %ld" RESET, line);
  add_spaces(sb, num_spaces);
  str_builder_add_format(sb, "│ %s", str_builder_peek(code));
}

// The lines around problem_line, numbered, with a caret under problem_col.
static void add_code_block(StrBuilder* sb, const char* source, size_t source_len,
  long problem_line, long problem_col) {
  long start_line = problem_line > 2 ? (problem_line - 2) : 0;
  long end_line = problem_line + 2;
  int max_num_places = num_places(end_line);

  StrBuilder* code = str_builder_create();

  bool in_block = false;
  size_t i = 0;
  long line = 0;
  char c;
  while(i < source_len) {
    c = source[i];

    if(line == start_line) {
      in_block = true;
    }

    if(in_block) {
      if(line > end_line) {
        in_block = false;
        break;
      }

      str_builder_add_char(code, c);
    }

    if(c == '\n') {
      if(in_block) {
        add_code_line(sb, code, line + 1, max_num_places);
        str_builder_clear(code);
      }

      if(line == problem_line) {
        add_spaces(sb, CODE_BLOCK_INDENT + max_num_places + 1 + problem_col + 1);
        str_builder_add_format(sb, BOLDRED "˄" RESET "\n");
      }

      line++;
    }

    i++;
  }

  // Want at least 4 lines, so print this if not.
  if(line <= 3) {
    add_code_line(sb, code, line + 1, max_num_places);
  }

  str_builder_add_char(sb, '\n');

  str_builder_destroy(code);
}

void diagnostic_list_add_error(DiagnosticList* list, const char* filename,
  const char* source, size_t source_len, long line, long column,
  const char* message) {
  StrBuilder* sb = str_builder_create();
  str_builder_add_format(sb, BOLDWHITE "%s" RESET ":%li:%li\n", filename, line + 1, column);
  str_builder_add_format(sb, "\n " BOLDRED "𝒙" RESET RED " %s\n\n" RESET, message);
  add_code_block(sb, source, source_len, line, column);
  str_builder_add_char(sb, '\n');

  add(list, LUCY_SEVERITY_ERROR, LUCY_STREAM_STDERR, xstrdup(message),
    line + 1, column, str_builder_release(sb, NULL));
}

void diagnostic_list_add_warning(DiagnosticList* list, LucyStream stream,
  const char* text) {
  size_t len = strlen(text);
  if(len > 0 && text[len - 1] == '\n') {
    len--;
  }
  char* message = xmalloc(size_add(len, 1));
  memcpy(message, text, len);
  message[len] = '\0';

  add(list, LUCY_SEVERITY_WARNING, stream, message, 0, 0, xstrdup(text));
}
