#pragma once

/*
 * The diagnostics a compile reports: its errors and warnings. The parser
 * and the printer add them to a DiagnosticList, which the LucyResult owns,
 * and the API in lucy.h hands them to the caller. The library never prints
 * them itself.
 *
 * Each diagnostic keeps its text exactly as lucyc prints it, ANSI colours
 * and all, alongside the plain message and position.
 */

#include <stddef.h>

#include "lucy.h"

struct LucyDiagnostic {
  LucySeverity severity;
  LucyStream stream;
  // malloc'd, like text.
  char* message;
  // The position the text shows, line counting from 1; 0 and 0 for none.
  long line;
  long column;
  char* text;
};

typedef struct DiagnosticList {
  LucyDiagnostic* items;
  size_t count;
  size_t capacity;
} DiagnosticList;

/* An empty list, which allocates nothing until it is added to. */
void diagnostic_list_init(DiagnosticList*);

/* Free every diagnostic and the list's storage, leaving it empty. */
void diagnostic_list_clear(DiagnosticList*);

/*
 * Add an error at line and column (both counting from 0) of source, read
 * from filename. Its text is a header with the position, the message, and
 * the lines of source around it with a caret under the column.
 */
void diagnostic_list_add_error(DiagnosticList*, const char* filename,
  const char* source, size_t source_len, long line, long column,
  const char* message);

/*
 * Add a warning whose text is text alone, to be printed to stream. Its
 * message is the text without a final newline.
 */
void diagnostic_list_add_warning(DiagnosticList*, LucyStream stream,
  const char* text);
