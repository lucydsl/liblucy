#include "printer.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void printer_warn(Printer* printer, LucyStream stream, const char* text) {
  diagnostic_list_add_warning(printer->diagnostics, stream, text);
}

void printer_warnf(Printer* printer, LucyStream stream, const char* format, ...) {
  va_list args;
  va_start(args, format);
  va_list args_copy;
  va_copy(args_copy, args);
  int len = vsnprintf(NULL, 0, format, args_copy);
  va_end(args_copy);
  // len is negative only for an encoding error; print nothing then.
  size_t size = len > 0 ? (size_t)len + 1 : 1;
  char* text = arena_alloc(printer->arena, size);
  if(len > 0) {
    vsnprintf(text, size, format, args);
  }
  va_end(args);
  printer_warn(printer, stream, text);
}

const char* printer_span(Printer* printer, SourceSpan span) {
  return arena_strndup(printer->arena, printer->source + span.start, span.end - span.start);
}

const char* printer_capitalize(Printer* printer, const char* name) {
  char* copy = arena_strdup(printer->arena, name);
  copy[0] = (char)toupper((unsigned char)copy[0]);
  return copy;
}

const char* printer_concat(Printer* printer, const char* a, const char* b) {
  size_t a_len = strlen(a);
  size_t b_len = strlen(b);
  char* out = arena_alloc(printer->arena, a_len + b_len + 1);
  memcpy(out, a, a_len);
  memcpy(out + a_len, b, b_len + 1);
  return out;
}
