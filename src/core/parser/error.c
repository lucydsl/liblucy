#include "internal.h"

#include <stdarg.h>
#include <stdio.h>

#include "../diagnostic.h"

static void report(Parser* parser, long line, long column, const char* message) {
  diagnostic_list_add_error(parser->diagnostics, parser->filename,
    parser->source, parser->source_len, line, column, message);
}

void parser_error(Parser* parser, const char* message) {
  // Known bug: this adds 1 to the line, which counts from 0, and the
  // error's header adds 1 again, so these errors point a line too far down.
  report(parser, (long)parser->line + 1, (long)parser->column, message);
}

void parser_error_back(Parser* parser, size_t back, const char* message) {
  Position pos;
  parser_find_position(parser, &pos, back);
  report(parser, pos.line, pos.column, message);
}

void parser_error_backf(Parser* parser, size_t back, const char* format, ...) {
  // Formatted messages have always been cut to 255 bytes.
  char message[256];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  parser_error_back(parser, back, message);
}

void parser_error_unexpected_identifier(Parser* parser) {
  parser_error(parser, "Unexpected identifier");
}
